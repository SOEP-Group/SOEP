#pragma once

#include <string>

namespace SOEP {
    class SatelliteProcessor {
    public:
        SatelliteProcessor(const std::string& apiKey, int numSatellites = 3000);
        ~SatelliteProcessor();

        void invoke();

    private:
        void fetchSatelliteTLEData(int id);
        void processSatelliteTLEData(int id, std::string& tle_data);

        std::string m_ApiKey;
        int m_NumSatellites;
    };
}