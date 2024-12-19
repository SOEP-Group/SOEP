#include "pch.h"
#include "satellite_processor.h"
#include "core/threadpool.h"
#include "database/pool/connection_pool.h"
#include "database/scoped_connection.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <optional>
#include <ctime>
#include <mutex>

namespace SOEP
{
    // Constants
    constexpr double MAX_STOP_TIME_MIN = 1440.0; // Maximum of 1 day

    static std::string convertToTimestamp(double tsince_min, const std::string &epoch)
    {
        std::istringstream ss(epoch);
        std::tm epochTime = {};
        ss >> std::get_time(&epochTime, "%Y-%m-%dT%H:%M:%SZ");
        if (ss.fail())
        {
            spdlog::error("Failed to parse epoch time: {}", epoch);
            return "";
        }

        std::time_t epochTimeT = timegm(&epochTime);
        if (epochTimeT == -1)
        {
            spdlog::error("Failed to convert epoch time");
            return "";
        }

        std::time_t updatedTime = epochTimeT + static_cast<std::time_t>(tsince_min * 60.0);

        std::tm *gmt = std::gmtime(&updatedTime);
        if (!gmt)
        {
            spdlog::error("Failed to convert updatedTime to tm structure.");
            return "";
        }

        char formattedTime[25];
        if (std::strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%dT%H:%M:%SZ", gmt) == 0)
        {
            spdlog::error("Failed to format updatedTime as ISO 8601.");
            return "";
        }

        return std::string(formattedTime);
    }

    static std::optional<std::string> extractEpochFromTLE(const std::string &tle_line1)
    {
        if (tle_line1.length() < 32)
        {
            spdlog::error("TLE line1 is too short to extract epoch.");
            return std::nullopt;
        }

        std::string epoch_year_str = tle_line1.substr(18, 2);
        int epoch_year;
        try
        {
            epoch_year = std::stoi(epoch_year_str);
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to parse epoch year from TLE line1: {}", e.what());
            return std::nullopt;
        }

        if (epoch_year < 57)
            epoch_year += 2000;
        else
            epoch_year += 1900;

        std::string epoch_day_str = tle_line1.substr(20, 12);
        double epoch_day;
        try
        {
            epoch_day = std::stod(epoch_day_str);
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to parse epoch day from TLE line1: {}", e.what());
            return std::nullopt;
        }

        int day_of_year = static_cast<int>(epoch_day);
        double fractional_day = epoch_day - day_of_year;

        std::tm epoch_tm = {};
        epoch_tm.tm_year = epoch_year - 1900;
        epoch_tm.tm_mon = 0;
        epoch_tm.tm_mday = 1;
        epoch_tm.tm_hour = 0;
        epoch_tm.tm_min = 0;
        epoch_tm.tm_sec = 0;
        epoch_tm.tm_isdst = 0;

        std::time_t base_time = timegm(&epoch_tm);
        if (base_time == -1)
        {
            spdlog::error("Failed to create base time for epoch year {}", epoch_year);
            return std::nullopt;
        }

        base_time += (day_of_year - 1) * 86400;

        int fractional_seconds = static_cast<int>(fractional_day * 86400.0 + 0.5);
        base_time += fractional_seconds;

        std::tm *gmt = std::gmtime(&base_time);
        if (!gmt)
        {
            spdlog::error("Failed to convert epoch_time_t to tm structure.");
            return std::nullopt;
        }

        char epochBuffer[25];
        if (std::strftime(epochBuffer, sizeof(epochBuffer), "%Y-%m-%dT%H:%M:%SZ", gmt) == 0)
        {
            spdlog::error("Failed to format epoch timestamp.");
            return std::nullopt;
        }

        return std::string(epochBuffer);
    }

    SatelliteProcessor::SatelliteProcessor(int num_satellites, int offset,
                                           double start_time, double stop_time, double step_size)
        : m_NumSatellites(num_satellites), m_Offset(offset),
          m_StartTime(start_time), m_StopTime(stop_time), m_StepSize(step_size) {}

    SatelliteProcessor::~SatelliteProcessor() {}

    void SatelliteProcessor::invoke()
    {
        if (!fetchNoradIds())
        {
            spdlog::debug("Process terminated. No TLE data found in the database.");
            return;
        }

        int numToProcess = static_cast<int>(m_NoradIds.size());
        spdlog::info("Processing {} satellites (Phase 1: fetch & process)", numToProcess);

        {
            // Define a thread pool with a controlled number of threads to prevent database overload
            // Adjust the number of threads based on your database's capacity
            SOEP::ThreadPool pool{15}; // Reduced from 30 to 10

            for (int i = 0; i < numToProcess; i++)
            {
                int satId = m_NoradIds[i];
                pool.AddTask([this, satId]()
                             {
                    auto result = this->processSatelliteFromDb(satId);
                    if (result.has_value())
                    {
                        // Insert the data immediately after propagation
                        this->insertSatelliteData(result.value());
                    } });
            }

            pool.Await();
            pool.Shutdown();
        }

        spdlog::info("Processing complete.");
    }

    bool SatelliteProcessor::fetchNoradIds()
    {
        spdlog::info("Fetching satellite IDs from the database");
        auto &connPool = ConnectionPool::getInstance();
        ScopedConnection conn(connPool);
        if (!conn)
        {
            spdlog::error("Failed to acquire DB connection when fetching IDs.");
            return false;
        }

        std::string query = "SELECT satellite_id FROM satellites ORDER BY satellite_id LIMIT " +
                            std::to_string(m_NumSatellites) + " OFFSET " + std::to_string(m_Offset) + ";";

        auto response = conn->executeSelectQuery(query);
        if (!response.success)
        {
            spdlog::error("Error fetching IDs: {}", response.errorMsg);
            spdlog::debug("Make sure the 'satellites' table exists and is populated in the database.");
            return false;
        }

        for (const auto &row : response.payload)
        {
            int id = std::stoi(row.at("satellite_id"));
            m_NoradIds.push_back(id);
        }
        if (m_NoradIds.empty())
        {
            spdlog::warn("No IDs fetched from the database. Check the OFFSET and NUM_SATELLITES values.");
            return false;
        }

        spdlog::info("Fetched {} satellite IDs from the database", m_NoradIds.size());
        return true;
    }

    std::optional<ProcessedSatelliteData> SatelliteProcessor::processSatelliteFromDb(int id)
    {
        auto &connPool = ConnectionPool::getInstance();
        ScopedConnection conn(connPool);
        if (!conn)
        {
            spdlog::error("Failed to acquire DB connection for satellite ID {}", id);
            return std::nullopt;
        }

        std::string query = "SELECT tle_line1, tle_line2 FROM satellite_data WHERE satellite_id = " + std::to_string(id) + ";";
        auto response = conn->executeSelectQuery(query);
        if (!response.success)
        {
            spdlog::error("Error fetching TLE data for satellite {}: {}", id, response.errorMsg);
            return std::nullopt;
        }

        if (response.payload.empty())
        {
            spdlog::warn("No TLE data found for satellite ID {}", id);
            return std::nullopt;
        }

        const std::string &tle_line1 = response.payload[0]["tle_line1"];
        const std::string &tle_line2 = response.payload[0]["tle_line2"];

        if (tle_line1.empty() || tle_line2.empty())
        {
            spdlog::error("TLE data is empty for satellite ID {}", id);
            return std::nullopt;
        }

        // Extract epoch from TLE line1
        auto epoch_opt = extractEpochFromTLE(tle_line1);
        if (!epoch_opt.has_value())
        {
            spdlog::error("Failed to extract epoch from TLE for satellite ID {}", id);
            return std::nullopt;
        }

        std::string epoch_timestamp = epoch_opt.value();

        // Parse epoch_timestamp to std::time_t
        std::tm epoch_tm = {};
        {
            std::istringstream ss(epoch_timestamp);
            ss >> std::get_time(&epoch_tm, "%Y-%m-%dT%H:%M:%SZ");
            if (ss.fail())
            {
                spdlog::error("Failed to parse epoch timestamp: {}", epoch_timestamp);
                return std::nullopt;
            }
        }
        std::time_t epoch_time_t = timegm(&epoch_tm);
        if (epoch_time_t == -1)
        {
            spdlog::error("Failed to convert epoch_time_t for satellite ID {}", id);
            return std::nullopt;
        }

        // Calculate stop_time_min as (now + 1 hour - epoch_time_t) / 60
        std::time_t now_plus_one_hour = std::time(nullptr) + 10800;
        double stop_time_min = difftime(now_plus_one_hour, epoch_time_t) / 60.0;

        // **Exclude satellites exceeding the propagation limit**
        if (stop_time_min > MAX_STOP_TIME_MIN)
        {
            spdlog::warn("Calculated stop_time_min ({}) exceeds MAX_STOP_TIME_MIN ({}). Excluding satellite ID {} from processing.",
                         stop_time_min, MAX_STOP_TIME_MIN, id);
            return std::nullopt; // Exclude this satellite from processing
        }

        // Ensure stop_time_min is positive
        if (stop_time_min <= 0)
        {
            spdlog::warn("Calculated stop_time_min ({}) is non-positive for satellite ID {}. Skipping propagation.", stop_time_min, id);
            return std::nullopt; // Exclude this satellite from processing
        }

        spdlog::debug("Propagating satellite ID {} with stop_time_min {}", id, stop_time_min);

        // **Process TLE Data Without Retry Mechanism**
        auto propagated_json = processSatelliteTLEData(id, tle_line1, tle_line2, stop_time_min, epoch_timestamp);
        if (!propagated_json.has_value())
        {
            spdlog::error("Failed to propagate satellite {}", id);
            return std::nullopt; // Exclude this satellite from processing
        }

        spdlog::info("Successfully propagated satellite ID {} from tsince_min 0 to {}", id, stop_time_min);

        return ProcessedSatelliteData{id, epoch_timestamp, std::move(propagated_json.value())};
    }

    std::optional<nlohmann::json> SatelliteProcessor::processSatelliteTLEData(int id, const std::string &tle_line1, const std::string &tle_line2, double stop_time_min, const std::string &epoch_timestamp)
    {
        if (stop_time_min <= 0)
        {
            spdlog::warn("stop_time_min is non-positive for satellite ID {}. Skipping propagation.", id);
            return std::nullopt;
        }

        std::ostringstream command;
        command << "python3 -c \"import sgp4_module; result = sgp4_module.propagate_satellite('"
                << tle_line1 << "', '" << tle_line2 << "', 0, " << stop_time_min
                << ", " << m_StepSize << "); print(result)\"";

        FILE *pipe = popen(command.str().c_str(), "r");
        if (!pipe)
        {
            spdlog::error("Failed to run Python script for satellite {}", id);
            return std::nullopt;
        }

        // Use RAII to ensure pipe is closed even if an exception occurs
        struct PipeCloser
        {
            FILE *pipe;
            PipeCloser(FILE *p) : pipe(p) {}
            ~PipeCloser()
            {
                if (pipe)
                    pclose(pipe);
            }
        } pipeCloser(pipe);

        char buffer[4096];
        std::string result;
        size_t maxSize = 100000000; // 100 MB
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            result += buffer;
            if (result.size() > maxSize)
            {
                spdlog::error("Output too large for satellite {}", id);
                return std::nullopt;
            }
        }

        // Check the return code of the Python script
        int returnCode = pclose(pipe);
        pipeCloser.pipe = nullptr; // Already closed by pclose

        if (returnCode != 0)
        {
            spdlog::error("Python script returned non-zero exit code {} for satellite ID {}", returnCode, id);
            return std::nullopt;
        }

        // Parse the JSON result
        nlohmann::json jsonResult;
        try
        {
            jsonResult = nlohmann::json::parse(result);
        }
        catch (const std::exception &e)
        {
            spdlog::error("Error parsing JSON result for satellite {}: {}", id, e.what());
            return std::nullopt;
        }

        // Check for errors in the JSON result
        if (jsonResult.is_array())
        {
            bool hasError = false;
            for (const auto &record : jsonResult)
            {
                if (record.contains("error"))
                {
                    spdlog::error("Propagation error for satellite ID {}: {}", id, record["error"].get<std::string>());
                    hasError = true;
                    break;
                }
            }

            if (hasError)
                return std::nullopt;
        }
        else if (jsonResult.contains("error"))
        {
            spdlog::error("Propagation error for satellite ID {}: {}", id, jsonResult["error"].get<std::string>());
            return std::nullopt;
        }

        // Add epoch to each record
        for (auto &rec : jsonResult)
        {
            rec["epoch"] = epoch_timestamp;
        }

        return jsonResult;
    }

    void SatelliteProcessor::insertSatelliteData(const ProcessedSatelliteData &data)
    {
        auto &connPool = ConnectionPool::getInstance();
        ScopedConnection conn(connPool);
        if (!conn)
        {
            spdlog::error("Failed to acquire DB connection for inserting satellite ID {}", data.satellite_id);
            return;
        }

        // Start a transaction for the insertion
        auto beginResponse = conn->beginTransaction();
        if (!beginResponse.success)
        {
            spdlog::error("Failed to start transaction for inserting satellite ID {}: {}", data.satellite_id, beginResponse.errorMsg);
            return;
        }

        std::ostringstream insertStream;
        insertStream << "INSERT INTO orbit_data (satellite_id, timestamp, x_km, y_km, z_km, "
                        "xdot_km_per_s, ydot_km_per_s, zdot_km_per_s) VALUES ";

        bool firstValue = true;

        for (const auto &record : data.propagated_data)
        {
            // Ensure all required fields are present
            if (!(record.contains("tsince_min") && record.contains("x_km") && record.contains("y_km") &&
                  record.contains("z_km") && record.contains("xdot_km_per_s") &&
                  record.contains("ydot_km_per_s") && record.contains("zdot_km_per_s") && record.contains("epoch")))
            {
                spdlog::error("Incomplete data for satellite {}: {}", data.satellite_id, record.dump());
                // Skip this record and continue with others
                continue;
            }

            double tsince_min = record["tsince_min"].get<double>();
            std::string timestamp = convertToTimestamp(tsince_min, record["epoch"].get<std::string>());
            if (timestamp.empty())
            {
                spdlog::error("Failed to convert tsince_min for satellite {}", data.satellite_id);
                // Skip this record and continue with others
                continue;
            }

            double x_km = record["x_km"].get<double>();
            double y_km = record["y_km"].get<double>();
            double z_km = record["z_km"].get<double>();
            double xdot_km_per_s = record["xdot_km_per_s"].get<double>();
            double ydot_km_per_s = record["ydot_km_per_s"].get<double>();
            double zdot_km_per_s = record["zdot_km_per_s"].get<double>();

            if (!firstValue)
            {
                insertStream << ", ";
            }
            else
            {
                firstValue = false;
            }

            insertStream << "("
                         << data.satellite_id << ", "
                         << "'" << timestamp << "', "
                         << x_km << ", "
                         << y_km << ", "
                         << z_km << ", "
                         << xdot_km_per_s << ", "
                         << ydot_km_per_s << ", "
                         << zdot_km_per_s << ")";
        }

        if (firstValue)
        {
            // No valid records to insert
            spdlog::warn("No valid records to insert for satellite ID {}", data.satellite_id);
            conn->rollbackTransaction();
            return;
        }

        insertStream << " ON CONFLICT (satellite_id, timestamp) DO UPDATE SET "
                        "x_km = EXCLUDED.x_km, "
                        "y_km = EXCLUDED.y_km, "
                        "z_km = EXCLUDED.z_km, "
                        "xdot_km_per_s = EXCLUDED.xdot_km_per_s, "
                        "ydot_km_per_s = EXCLUDED.ydot_km_per_s, "
                        "zdot_km_per_s = EXCLUDED.zdot_km_per_s;";

        auto updateResponse = conn->executeUpdateQuery(insertStream.str());
        if (!updateResponse.success)
        {
            spdlog::error("Insertion query failed for satellite ID {}: {}", data.satellite_id, updateResponse.errorMsg);
            auto rollbackResponse = conn->rollbackTransaction();
            if (!rollbackResponse.success)
                spdlog::error("Failed to rollback transaction for satellite ID {}: {}", data.satellite_id, rollbackResponse.errorMsg);
            return;
        }

        auto commitResponse = conn->commitTransaction();
        if (!commitResponse.success)
        {
            spdlog::error("Failed to commit transaction for satellite ID {}: {}", data.satellite_id, commitResponse.errorMsg);
            auto rollbackResponse = conn->rollbackTransaction();
            if (!rollbackResponse.success)
                spdlog::error("Failed to rollback transaction for satellite ID {}: {}", data.satellite_id, rollbackResponse.errorMsg);
            return;
        }

        spdlog::info("Successfully inserted data for satellite ID {}", data.satellite_id);
    }
}
