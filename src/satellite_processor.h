#pragma once

namespace SOEP {
    class SatelliteProcessor {
    public:
        SatelliteProcessor(const std::string& apiKey);
        ~SatelliteProcessor();

        void invoke();

    private:
        void fetchSatelliteTLEData(int id);
        void processSatelliteTLEData(int id, std::string& TLEData);

        std::string m_ApiKey;
        std::string saveToDb(const std::string data);
    };
}