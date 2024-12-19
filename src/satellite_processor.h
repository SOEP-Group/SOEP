// satellite_processor.h
#pragma once

#include <vector>
#include <string>
#include <optional>
#include <mutex>

// Include nlohmann::json library
#include <nlohmann/json.hpp>

namespace SOEP
{
    // Structure to hold processed satellite data
    struct ProcessedSatelliteData
    {
        int satellite_id;
        std::string epoch_timestamp;
        nlohmann::json propagated_data;
    };

    // SatelliteProcessor Class
    class SatelliteProcessor
    {
    public:
        // Constructor
        SatelliteProcessor(int num_satellites, int offset,
                           double start_time, double stop_time, double step_size);

        // Destructor
        ~SatelliteProcessor();

        // Main method to invoke processing
        void invoke();

    private:
        // Fetch NORAD IDs from the database
        bool fetchNoradIds();

        // Process a single satellite from the database
        std::optional<ProcessedSatelliteData> processSatelliteFromDb(int id);

        std::optional<nlohmann::json> processSatelliteTLEData(int id, const std::string &tle_line1,
                                                              const std::string &tle_line2,
                                                              double stop_time_min,
                                                              const std::string &epoch_timestamp);

        void insertSatelliteData(const ProcessedSatelliteData &data);

        int m_NumSatellites;
        int m_Offset;
        double m_StartTime;
        double m_StopTime;
        double m_StepSize;

        std::vector<int> m_NoradIds;

        // Mutex for thread-safe operations (if needed)
        std::mutex m_InsertMutex;
    };
}
