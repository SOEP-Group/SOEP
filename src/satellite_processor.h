#pragma once

namespace SOEP {
    class SatelliteProcessor {
    public:
        SatelliteProcessor(const std::string& apiKey);
        ~SatelliteProcessor();

        void invoke();

    private:
        void fetchSatelliteTLEData(int id);
        void processSatelliteTLEData(int id, std::string& tle_data);

        std::string m_ApiKey;
    };
}