#pragma once

namespace SOEP {
    class SatelliteProcessor {
    public:
        SatelliteProcessor(const std::string& apiKey);
        ~SatelliteProcessor();


    private:
        std::string m_ApiKey;
    };
}