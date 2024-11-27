#pragma once

#include <string>

namespace SOEP {
    class SatelliteProcessor {
    public:
        SatelliteProcessor(const std::string& apiKey, int num_satellites = 1000, int offset = 0);
        ~SatelliteProcessor();

        void invoke();

    private:
        bool fetchNoradIds();
        void fetchSatelliteTLEData(int id);
        void processSatelliteTLEData(int id, std::string& tle_data);

        std::string m_ApiKey;
        int m_NumSatellites;
        int m_Offset;
        std::vector<int> m_NoradIds;
    };
}