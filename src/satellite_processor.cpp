#include "pch.h"
#include "satellite_processor.h"
#include "network/network.h"
#include "core/threadpool.h"
#include "database/pool/connection_pool.h"
#include "db_RAII.h"


namespace SOEP {
    SatelliteProcessor::SatelliteProcessor(const std::string& apiKey, int numSatellites, int offset)
        : m_ApiKey(apiKey), m_NumSatellites(numSatellites), m_Offset(offset) {}

    SatelliteProcessor::~SatelliteProcessor() {}

    void SatelliteProcessor::invoke() {
        SOEP::ThreadPool pool{ 20 };

        if (!fetchNoradIds()) {
            spdlog::debug("process terminated. no API calls or alterations in the db was made");
            return;
        }

        int numToProcess = static_cast<int>(m_NoradIds.size());

        spdlog::info("processing {} satellites", numToProcess);
        for (int i = 0; i < numToProcess; i++) {
            pool.AddTask([this, id = m_NoradIds[i]]() {
                this->fetchSatelliteTLEData(id);
            });
        }

        pool.Await();
        pool.Shutdown();
    }

    bool SatelliteProcessor::fetchNoradIds() {
        auto& connPool = ConnectionPool::getInstance();
        ScopedDbConn dbConn(connPool);
        auto conn = dbConn.get();
        if (!conn) {
            spdlog::error("failed to aquire db connection when fetching ids");
            return false;
        }

        std::string query = "SELECT satellite_id FROM satellites ORDER BY satellite_id LIMIT " +
                            std::to_string(m_NumSatellites) + " OFFSET " + std::to_string(m_Offset) + ";";

        try {
            auto res = conn->executeSelectQuery(query);
            for (const auto& row : res) {
                int id = std::stoi(row.at("satellite_id"));
                m_NoradIds.push_back(id);
            }
            if (m_NoradIds.empty()) {
                spdlog::warn("no ids fetched from db. check the OFFSET and NUM_SATELLITES values");
                return false;
            }
        } catch (const std::exception& e) {
            spdlog::error("error fetching ids: {}", e.what());
            spdlog::debug("make sure 'satellites' table exist and is populated in the db");
            return false;
        }

        return true;
    }

    void SatelliteProcessor::fetchSatelliteTLEData(int id) {
        std::string url = "https://api.n2yo.com/rest/v1/satellite/tle/" +
                        std::to_string(id) + "/" +
                        "&apiKey=" + m_ApiKey;

        Network::Call(url, [this, id](std::shared_ptr<std::string> result) {
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
            }
        }, nullptr, nullptr);
    }


    void SatelliteProcessor::processSatelliteTLEData(int id, std::string& tle_data) {
        size_t pos = tle_data.find("\r\n");
        if (pos == std::string::npos) {
            spdlog::error("Invalid TLE data format for satellite: {}", id);
            return;
        }

        std::string tle_line1 = tle_data.substr(0, pos);
        std::string tle_line2 = tle_data.substr(pos + 2);

        std::replace(tle_line1.begin(), tle_line1.end(), '\'', '\"');
        std::replace(tle_line2.begin(), tle_line2.end(), '\'', '\"');

        std::ostringstream command;
        command << "python3 -c \"import sgp4_module; result = sgp4_module.propagate_satellite('"
                << tle_line1 << "', '" << tle_line2 << "', 0, 1440, 1); print(result)\"";

        FILE* pipe = popen(command.str().c_str(), "r");
        if (!pipe) {
            spdlog::error("Failed to run Python script for satellite {}", id);
            return;
        }

        char buffer[128];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }

        int returnCode = pclose(pipe);
        if (returnCode != 0) {
            spdlog::error("Error running Python script for satellite {}", id);
        }

        nlohmann::json jsonResult;
        try {
            jsonResult = nlohmann::json::parse(result);
        } catch (const std::exception& e) {
            spdlog::error("Error parsing JSON result for satellite {}: {}", id, e.what());
            return;
        }

        auto currentTime = std::chrono::system_clock::now();

        std::vector<nlohmann::json> recordsWithTimestamps;
        for (const auto& record : jsonResult) {
            // Check for required keys in each record
            if (!(record.contains("tsince_min") && record.contains("x_km") && record.contains("y_km") &&
                record.contains("z_km") && record.contains("xdot_km_per_s") &&
                record.contains("ydot_km_per_s") && record.contains("zdot_km_per_s"))) {
                spdlog::error("Incomplete data for satellite {}: {}", id, record.dump());
                continue;
            }

            // Calculate the timestamp based on `tsince_min`
            auto recordTime = currentTime + std::chrono::minutes(static_cast<int>(record["tsince_min"].get<double>()));
            std::time_t recordTimeT = std::chrono::system_clock::to_time_t(recordTime);
            std::tm* gmt = std::gmtime(&recordTimeT);

            std::ostringstream timeStream;
            timeStream << std::put_time(gmt, "%Y-%m-%d %H:%M");

            nlohmann::json recordWithTimestamp = record;
            recordWithTimestamp["timestamp"] = timeStream.str();
            recordsWithTimestamps.push_back(recordWithTimestamp);
        }

        auto& connPool = ConnectionPool::getInstance();
        ScopedDbConn dbConn(connPool);
        auto conn = dbConn.get();
        if (!conn) {
            spdlog::error("Failed to acquire db connection for satellite: {}", id);
            return;
        }

        try {
            conn->beginTransaction();

            for (const auto& record : recordsWithTimestamps) {
                conn->executeUpdateQuery(
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
                    record["zdot_km_per_s"].get<double>()
                );
            }

            conn->commitTransaction();
        } catch (const std::exception& e) {
            conn->rollbackTransaction();
            spdlog::error("Database transaction failed for satellite {}: {}", id, e.what());
        }
    }
}

