#include "satellite_processor.h"
#include "network/network.h"
#include "core/threadpool.h"
#include "database/pool/connection_pool.h"
#include "db_RAII.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>

namespace SOEP {
    SatelliteProcessor::SatelliteProcessor(const std::string& apiKey) : m_ApiKey(apiKey) {}

    SatelliteProcessor::~SatelliteProcessor() {}

     void SatelliteProcessor::invoke(){
        SOEP::ThreadPool pool{ 10 };

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

        for (const auto& id : noradJson) {
            pool.AddTask([this, id]() {
                this->fetchSatelliteTLEData(id);
            });
        }


    }

    void SatelliteProcessor::fetchSatelliteTLEData(int id) {

        std::string url = "https://api.n2yo.com/rest/v1/satellite/tle/" +
                        std::to_string(id) + "/" +
                        "&apiKey=" + m_ApiKey;

        Network::Call(url, [this, id](std::shared_ptr<std::string> result) {
            if (result && !result->empty()) {
                this->processSatelliteTLEData(id, *result);
            } else {
                spdlog::error("failed fetching TLE data from id {}", id);
            }
        }, nullptr, nullptr);
    }

    void SatelliteProcessor::processSatelliteTLEData(int id, std::string& TLEData) {

        // sgp4 bs

        // db

        auto& connPool = ConnectionPool::getInstance();
        ScopedDbConn scope(connPool);
        auto dbConn = scope.get();

        std::string query = "";

        if (dbConn) {
            dbConn->executeUpdateQuery(query);
        } else {
            spdlog::error("failed to acquire database connection");
        }
    }
}