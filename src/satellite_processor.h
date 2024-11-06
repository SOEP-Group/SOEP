#pragma once

#include <string>

namespace SOEP {
    class SatelliteProcessor {
    public:
        SatelliteProcessor(const std::string& apiKey, int numSatellites = 1000, int offset = 0,
                           double start_time = 0, double stop_time = 1440, double step_size = 1);
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

        double m_StartTime;
        double m_StopTime;
        double m_StepSize;
    };
}