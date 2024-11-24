#include "pch.h"
#include "satellite_processor.h"
#include "network/network.h"
#include "core/threadpool.h"
#include "database/pool/connection_pool.h"
#include "database/scoped_connection.h"

namespace SOEP
{
    SatelliteProcessor::SatelliteProcessor(const std::string &apiKey, int num_satellites, int offset,
                                           double start_time, double stop_time, double step_size)
        : m_ApiKey(apiKey), m_NumSatellites(num_satellites), m_Offset(offset),
          m_StartTime(start_time), m_StopTime(stop_time), m_StepSize(step_size) {}

    SatelliteProcessor::~SatelliteProcessor() {}

    void SatelliteProcessor::invoke()
    {
        SOEP::ThreadPool pool{20};

        if (!fetchNoradIds())
        {
            spdlog::debug("process terminated. no API calls or alterations in the db was made");
            return;
        }

        int numToProcess = static_cast<int>(m_NoradIds.size());

        spdlog::info("processing {} satellites", numToProcess);
        for (int i = 0; i < numToProcess; i++)
        {
            pool.AddTask([this, id = m_NoradIds[i]]()
                         { this->fetchSatelliteTLEData(id); });
        }

        pool.Await();
        pool.Shutdown();
    }

    bool SatelliteProcessor::fetchNoradIds()
    {
        auto &connPool = ConnectionPool::getInstance();
        ScopedConnection conn(connPool);
        if (!conn)
        {
            spdlog::error("failed to aquire db connection when fetching ids");
            return false;
        }

        std::string query = "SELECT satellite_id FROM satellites ORDER BY satellite_id LIMIT " +
                            std::to_string(m_NumSatellites) + " OFFSET " + std::to_string(m_Offset) + ";";

        auto response = conn->executeSelectQuery(query);
        if (!response.success)
        {
            spdlog::error("error fetching ids: {}", response.errorMsg);
            spdlog::debug("make sure 'satellites' table exist and is populated in the db");
            return false;
        }

        for (const auto &row : response.payload)
        {
            int id = std::stoi(row.at("satellite_id"));
            m_NoradIds.push_back(id);
        }
        if (m_NoradIds.empty())
        {
            spdlog::warn("no ids fetched from db. check the OFFSET and NUM_SATELLITES values");
            return false;
        }

        return true;
    }

    void SatelliteProcessor::fetchSatelliteTLEData(int id)
    {
        std::string url = "https://api.n2yo.com/rest/v1/satellite/tle/" +
                          std::to_string(id) + "/" +
                          "&apiKey=" + m_ApiKey;

        Network::Call(url, [this, id](std::shared_ptr<std::string> result)
                      {
            if (result && !result->empty()) {
                nlohmann::json jsonResponse;
                std::string tle_data;
                try {
                    jsonResponse = nlohmann::json::parse(*result);
                    tle_data = jsonResponse["tle"];
                } catch (const std::exception& e) {
                    spdlog::error("Error parsing TLE data from id {}: {}", id, e.what());
                }
                this->processSatelliteTLEData(id, tle_data);
            } else {
                spdlog::error("Failed fetching TLE data for id {}", id);
            } }, nullptr, nullptr);
    }

    void SatelliteProcessor::processSatelliteTLEData(int id, std::string &tle_data)
    {
        size_t pos = tle_data.find("\r\n");
        if (pos == std::string::npos)
        {
            spdlog::error("Invalid TLE data format for satellite: {}", id);
            return;
        }

        std::string tle_line1 = tle_data.substr(0, pos);
        std::string tle_line2 = tle_data.substr(pos + 2);

        std::replace(tle_line1.begin(), tle_line1.end(), '\'', '\"');
        std::replace(tle_line2.begin(), tle_line2.end(), '\'', '\"');

        std::ostringstream command;
        // args: tle_line1: str, tle_line2: str, start_time: float, stop_time: float, step_size: float
        command << "python3 -c \"import sgp4_module; result = sgp4_module.propagate_satellite('"
                << tle_line1 << "', '" << tle_line2 << "', " << m_StartTime << ", " << m_StopTime
                << ", " << m_StepSize << "); print(result)\"";

        FILE *pipe = popen(command.str().c_str(), "r");
        if (!pipe)
        {
            spdlog::error("Failed to run Python script for satellite {}", id);
            return;
        }

        char buffer[128];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            result += buffer;
        }

        int returnCode = pclose(pipe);
        if (returnCode != 0)
        {
            spdlog::error("Error running Python script for satellite {}", id);
        }

        nlohmann::json jsonResult;
        try
        {
            jsonResult = nlohmann::json::parse(result);
        }
        catch (const std::exception &e)
        {
            spdlog::error("Error parsing JSON result for satellite {}: {}", id, e.what());
            return;
        }

        std::string epoch_str = tle_line1.substr(18, 14);
        int epoch_year = std::stoi(epoch_str.substr(0, 2));
        double epoch_day = std::stod(epoch_str.substr(2));

        epoch_year += (epoch_year < 57) ? 2000 : 1900; // TLE epoch years from 1957 onwards

        std::tm tle_epoch_tm = {};
        tle_epoch_tm.tm_year = epoch_year - 1900;
        tle_epoch_tm.tm_mon = 0; // January
        tle_epoch_tm.tm_mday = 1;
        tle_epoch_tm.tm_hour = 0;
        tle_epoch_tm.tm_min = 0;
        tle_epoch_tm.tm_sec = 0;

        std::time_t tle_epoch_time = timegm(&tle_epoch_tm);

        int day_of_year = static_cast<int>(epoch_day);
        double fractional_day = epoch_day - day_of_year;

        tle_epoch_time += (day_of_year - 1) * 86400;
        tle_epoch_time += static_cast<std::time_t>(fractional_day * 86400);

        std::vector<nlohmann::json> recordsWithTimestamps;
        for (const auto &record : jsonResult)
        {
            if (!(record.contains("tsince_min") && record.contains("x_km") && record.contains("y_km") &&
                  record.contains("z_km") && record.contains("xdot_km_per_s") &&
                  record.contains("ydot_km_per_s") && record.contains("zdot_km_per_s")))
            {
                spdlog::error("Incomplete data for satellite {}: {}", id, record.dump());
                continue;
            }

            double tsince_min = record["tsince_min"].get<double>();
            std::time_t recordTimeT = tle_epoch_time + static_cast<std::time_t>(tsince_min * 60);

            std::tm gmt = {};
#if defined(_WIN32) || defined(_WIN64)
            errno_t err = gmtime_s(&gmt, &recordTimeT);
            if (err != 0)
            {
                spdlog::error("gmtime_s failed for satellite {}: invalid time", id);
                continue;
            }
#else
            if (gmtime_r(&recordTimeT, &gmt) == nullptr)
            {
                spdlog::error("gmtime_r failed for satellite {}: invalid time", id);
                continue;
            }
#endif

            char timeBuffer[20];
            if (std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M", &gmt) == 0)
            {
                spdlog::error("strftime failed for satellite {}: buffer too small", id);
                continue;
            }

            nlohmann::json recordWithTimestamp = record;
            recordWithTimestamp["timestamp"] = std::string(timeBuffer);
            recordsWithTimestamps.push_back(recordWithTimestamp);
        }

        int successfulInserts = 0;
        int failedInserts = 0;
        std::vector<std::string> errorMessages;

        auto &connPool = ConnectionPool::getInstance();
        ScopedConnection conn(connPool);
        if (!conn)
        {
            spdlog::error("Failed to acquire db connection for satellite: {}", id);
            return;
        }

        auto beginResponse = conn->beginTransaction();
        if (!beginResponse.success)
        {
            spdlog::error("failed to start transaction for satellite: {}: {}", id, beginResponse.errorMsg);
            return;
        }

        spdlog::debug("Transaction started for satellite {}", id);

        bool transactionFailed = false;

        for (const auto &record : recordsWithTimestamps)
        {
            auto updateResponse = conn->executeUpdateQuery(
                "INSERT INTO satellite_data (satellite_id, timestamp, x_km, y_km, z_km, "
                "xdot_km_per_s, ydot_km_per_s, zdot_km_per_s) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, $8) "
                "ON CONFLICT (satellite_id, timestamp) DO UPDATE SET "
                "x_km = EXCLUDED.x_km, "
                "y_km = EXCLUDED.y_km, "
                "z_km = EXCLUDED.z_km, "
                "xdot_km_per_s = EXCLUDED.xdot_km_per_s, "
                "ydot_km_per_s = EXCLUDED.ydot_km_per_s, "
                "zdot_km_per_s = EXCLUDED.zdot_km_per_s;",
                id,
                record["timestamp"].get<std::string>(),
                record["x_km"].get<double>(),
                record["y_km"].get<double>(),
                record["z_km"].get<double>(),
                record["xdot_km_per_s"].get<double>(),
                record["ydot_km_per_s"].get<double>(),
                record["zdot_km_per_s"].get<double>());

            if (!updateResponse.success)
            {
                errorMessages.push_back(updateResponse.errorMsg);
                failedInserts++;
                transactionFailed = true;
                break;
            }
            else
            {
                successfulInserts += updateResponse.payload;
            }
        }

        if (failedInserts == 0)
        {
            auto commitResponse = conn->commitTransaction();
            if (commitResponse.success)
            {
                spdlog::info("Satellite {}: {} records successfully inserted/updated.", id, successfulInserts);
                spdlog::debug("Transaction committed for satellite {}", id);
            }
            else
            {
                spdlog::error("Failed to commit transaction for satellite {}: {}", id, commitResponse.errorMsg);
                auto rollbackResponse = conn->rollbackTransaction();
                if (rollbackResponse.success)
                {
                    spdlog::debug("Transaction rolled back for satellite {}", id);
                }
                else
                {
                    spdlog::error("Failed to rollback transaction for satellite {}: {}", id, rollbackResponse.errorMsg);
                }
            }
        }
        else
        {
            conn->rollbackTransaction();

            std::string errorFileName = "satellite_" + std::to_string(id) + "_errors.log";
            std::ofstream errorFile(errorFileName, std::ios::app);
            if (errorFile.is_open())
            {
                for (const auto &msg : errorMessages)
                {
                    errorFile << msg << std::endl;
                }
                errorFile.close();
                spdlog::warn("Satellite {}: {} records failed to insert/update. Errors saved to {}.", id, failedInserts, errorFileName);
            }
            else
            {
                spdlog::error("Failed to open error log file for satellite {}.", id);
            }

            if (successfulInserts > 0)
            {
                spdlog::info("Satellite {}: {} records successfully inserted/updated.", id, successfulInserts);
            }
        }
    }
}
