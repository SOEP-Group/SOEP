#include "satellite_processor.h"
#include "network/network.h"
#include "core/threadpool.h"
#include "database/pool/connection_pool.h"
#include "db_RAII.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>

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
                try {
                    jsonResponse = nlohmann::json::parse(*result);
                    std::string tle_data = jsonResponse["tle"];
                    this->processSatelliteTLEData(id, tle_data);
                } catch (const std::exception& e) {
                    spdlog::error("Error parsing TLE data from id {}: {}", id, e.what());
                }
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
        command << "python3 -c \"import sgp4_module; result = sgp4_module.propagate_satellite('"
                << tle_line1 << "', '" << tle_line2 << "', 0, 1440, 1); print(result)\"";

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
        
        // parse result and store in db
        std::istringstream ss(result);
        std::string line;

        if (!std::getline(ss, line)) {
            spdlog::error("No data for satellite: {}", id);
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

            while (std::getline(ss, line)) {
                if (line.empty()) continue;

                std::istringstream lineStream(line);
                std::string token;
                std::vector<double> values;
                while (std::getline(lineStream, token, ',')) {
                    values.push_back(std::stod(token));
                }

                if (values.size() != 7) {
                    spdlog::error("wrong data format for satellite {}: {}", id, line);
                    continue;
                }

                std::string query = "INSERT INTO satellite_data (satellite_id, tsince_min, x_km, y_km, z_km, xdot_km_per_s, ydot_km_per_s, zdot_km_per_s) "
                                    "VALUES ($1, $2, $3, $4, $5, $6, $7, $8);";
                conn->executeUpdateQuery(query, id, values[0], values[1], values[2], values[3], values[4], values[5], values[6]);
            }

            conn->commitTransaction();
            spdlog::info("stored data for satellite: {}", id);
        } catch (const std::exception& e) {
            conn->rollbackTransaction();
            spdlog::error("db error for satellite: {}: {}", id, e.what());
        }
    }
}