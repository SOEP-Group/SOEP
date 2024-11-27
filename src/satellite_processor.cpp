#include "pch.h"
#include "satellite_processor.h"
#include "network/network.h"
#include "core/threadpool.h"
#include "database/pool/connection_pool.h"
#include "database/scoped_connection.h"


namespace SOEP {
    SatelliteProcessor::SatelliteProcessor(const std::string& apiKey, int num_satellites, int offset)
        : m_ApiKey(apiKey), m_NumSatellites(num_satellites), m_Offset(offset) {}

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
        ScopedConnection conn(connPool);
        if (!conn) {
            spdlog::error("failed to aquire db connection when fetching ids");
            return false;
        }

        std::string query = "SELECT satellite_id FROM satellites ORDER BY satellite_id LIMIT " +
                            std::to_string(m_NumSatellites) + " OFFSET " + std::to_string(m_Offset) + ";";

        auto response = conn->executeSelectQuery(query);
        if (!response.success) {
            spdlog::error("error fetching ids: {}", response.errorMsg);
            spdlog::debug("make sure 'satellites' table exist and is populated in the db");
            return false;
        }

        for (const auto& row : response.payload) {
            int id = std::stoi(row.at("satellite_id"));
            m_NoradIds.push_back(id);
        }
        if (m_NoradIds.empty()) {
            spdlog::warn("no ids fetched from db. check the OFFSET and NUM_SATELLITES values");
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

        auto& connPool = ConnectionPool::getInstance();
        ScopedConnection conn(connPool);
        if (!conn) {
            spdlog::error("Failed to acquire db connection for satellite: {}", id);
            return;
        }

        auto queryResponse = conn->executeUpdateQuery(
            "INSERT INTO satellite_data (satellite_id, tle_line1, tle_line2) "
            "VALUES ($1, $2, $3) "
            "ON CONFLICT (satellite_id) DO UPDATE SET "
            "tle_line1 = EXCLUDED.tle_line1, "
            "tle_line2 = EXCLUDED.tle_line2;",
            id,
            tle_line1,
            tle_line2
        );

        if (queryResponse.success) {
            spdlog::info("tle data updated for satellite: {} with {} line updated", id, queryResponse.payload);
        } else {
            spdlog::error("failed to update TLE data for satellite {}: {}", id, queryResponse.errorMsg);
        }
    }
}
