#include "satellite_processor.h"
#include "network/network.h"
#include "core/threadpool.h"
#include "database/pool/connection_pool.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace SOEP {
    SatelliteProcessor::SatelliteProcessor(const std::string& apiKey) : m_ApiKey(apiKey) {}

    SatelliteProcessor::~SatelliteProcessor() {}

    void SatelliteProcessor::invoke() {}

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

        auto dbConn = ConnectionPool::getInstance().acquire();

        std::string query = "";

        if (dbConn) {
            dbConn->executeUpdateQuery(query);
            ConnectionPool::getInstance().release(dbConn);
        } else {
            spdlog::error("failed to acquire database connection");
        }
    }
}