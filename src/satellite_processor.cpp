#include "satellite_processor.h"
#include "network/network.h"
#include "core/threadpool.h"
#include "database/pool/connection_pool.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace SOEP {
    SatelliteProcessor::SatelliteProcessor(const std::string& apiKey) : m_ApiKey(apiKey) {}

    SatelliteProcessor::~SatelliteProcessor() {}
}