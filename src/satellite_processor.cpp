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
#include <cstdlib>

namespace SOEP {
    SatelliteProcessor::SatelliteProcessor(const std::string& apiKey) : m_ApiKey(apiKey) {}

    SatelliteProcessor::~SatelliteProcessor() {}

     void SatelliteProcessor::invoke(){
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

        //take first 10 ids

        for (int i = 0; i < 100; i++) {
            pool.AddTask([this, id = noradJson[i]]() {
                this->fetchSatelliteTLEData(id);
            });
        }

        // for (const auto& id : noradJson) {
        //     pool.AddTask([this, id]() {
        //         this->fetchSatelliteTLEData(id);
        //     });
        // }


    }

    void SatelliteProcessor::fetchSatelliteTLEData(int id) {

        std::string url = "https://api.n2yo.com/rest/v1/satellite/tle/" +
                        std::to_string(id) + "/" +
                        "&apiKey=" + m_ApiKey;

        Network::Call(url, [this, id](std::shared_ptr<std::string> result) {
            if (result && !result->empty()) {
                // Parse the JSON response
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

         // Extract TLE lines from the response
    std::string tle_line1 = tle_data.substr(0, tle_data.find("\r\n"));  // first line
    std::string tle_line2 = tle_data.substr(tle_data.find("\r\n") + 2); // second line

    // Sanitize TLE lines by escaping problematic characters
    std::replace(tle_line1.begin(), tle_line1.end(), '\'', '\"');  // Escape single quotes
    std::replace(tle_line2.begin(), tle_line2.end(), '\'', '\"');  // Escape single quotes

    // Construct the Python command with escaped TLE data
    std::ostringstream command;
    command << "python3 -c \"import sgp4_module; result = sgp4_module.propagate_satellite('"
            << tle_line1 << "', '" << tle_line2 << "', 0, 1440, 1); print(result)\"";

    // Use popen to execute the command and capture output
    FILE* pipe = popen(command.str().c_str(), "r");
    if (!pipe) {
        spdlog::error("Failed to run Python script for satellite {}", id);
        return;
    }

    // Capture output from Python script
    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    // Close the pipe and log the result
    int returnCode = pclose(pipe);
    if (returnCode != 0) {
        spdlog::error("Error running Python script for satellite {}", id);
    } else {
        spdlog::info("Satellite {} propagation result: {}", id, result);
    }
        

        

        // auto& connPool = ConnectionPool::getInstance();
        // ScopedDbConn scope(connPool);
        // auto dbConn = scope.get();

        // std::string query = "";

        // if (dbConn) {
        //     dbConn->executeUpdateQuery(query);
        // } else {
        //     spdlog::error("failed to acquire database connection");
        // }
    }
}