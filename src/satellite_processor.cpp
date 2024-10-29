#include "pch.h"
#include "satellite_processor.h"
#include "network/network.h"
#include "core/threadpool.h"
#include "database/pool/connection_pool.h"
#include "db_RAII.h"

namespace SOEP {
    SatelliteProcessor::SatelliteProcessor(const std::string& apiKey, int numSatellites)
        : m_ApiKey(apiKey), m_NumSatellites(numSatellites) {}

    SatelliteProcessor::~SatelliteProcessor() {}

    void SatelliteProcessor::invoke() {
        SOEP::ThreadPool pool{ 20 };

        std::string jsonFilePath = "./resources/norad_ids.json"; 
        
        std::ifstream jsonFile(jsonFilePath);
        if (!jsonFile.is_open()) {
            spdlog::error("Failed to open NORAD IDs JSON file: {}", jsonFilePath);
            return;
        }

        nlohmann::json noradJson;
        try {
            jsonFile >> noradJson;
        } catch (const std::exception& e) {
            spdlog::error("Error parsing JSON file: {}", e.what());
            return;
        }

        if (!noradJson.is_array()) {
            spdlog::error("JSON format is invalid, expected an array of NORAD IDs.");
            return;
        }

        int numToProcess = std::min(m_NumSatellites, static_cast<int>(noradJson.size()));

        for (int i = 0; i < numToProcess; i++) {
            pool.AddTask([this, id = noradJson[i]]() {
                this->fetchSatelliteTLEData(id);
            });
        }

        pool.Await();
        pool.Shutdown();
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

        // sanitize TLE lines
        std::replace(tle_line1.begin(), tle_line1.end(), '\'', '\"');
        std::replace(tle_line2.begin(), tle_line2.end(), '\'', '\"');

        // construct the Python command
        std::ostringstream command;

        // args: tle_line1: str, tle_line2: str, start_time: float, stop_time: float, step_size: float
        command << "python3 -c \"import sgp4_module; result = sgp4_module.propagate_satellite('"
                << tle_line1 << "', '" << tle_line2 << "', 0, 10, 1); print(result)\""; // stop_time = 1440

        // execute the command and capture output
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
        } /*else {
            spdlog::info("Satellite {} propagation result: {}", id, result);
        }*/

        // parse and store data
        nlohmann::json jsonResult;
        try {
            jsonResult = nlohmann::json::parse(result);
        } catch (const std::exception& e) {
            spdlog::error("Error parsing JSON result for satellite {}: {}", id, e.what());
            return;
        }

        auto& connPool = ConnectionPool::getInstance();
        ScopedDbConn dbConn(connPool);
        auto conn = dbConn.get();
        if (!conn) {
            spdlog::error("failed to aquire db connection for satellite: {}", id);
            return;
        }

        try {
            conn->beginTransaction();

            for (const auto& record : jsonResult) {
                if (!record.contains("tsince_min") || !record.contains("x_km") || !record.contains("y_km") ||
                    !record.contains("z_km") || !record.contains("xdot_km_per_s") ||
                    !record.contains("ydot_km_per_s") || !record.contains("zdot_km_per_s")) {
                    spdlog::error("wrong data format for satellite {}: {}", id, record.dump());
                    continue;
                }

                conn->executeUpdateQuery(
                    "INSERT INTO satellite_data (satellite_id, tsince_min, x_km, y_km, z_km, "
                    "xdot_km_per_s, ydot_km_per_s, zdot_km_per_s) "
                    "VALUES ($1, $2, $3, $4, $5, $6, $7, $8);",
                    id,
                    record["tsince_min"].get<double>(),
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
        }
    }
}