#pragma once

#include <map>
#include <vector>
#include <string>
#include <optional>
#include <utility>

struct SatelliteState
{
    int satellite_id;
    double x_km;
    double y_km;
    double z_km;
    double vx_km_s;
    double vy_km_s;
    double vz_km_s;
};

namespace SOEP
{
    class CollisionProbabilityCalculator
    {
    public:
        CollisionProbabilityCalculator();
        ~CollisionProbabilityCalculator(); // Destructor declared normally

        std::map<int, SatelliteState> retrieveSatelliteStates(const std::string &timestamp);

        std::optional<std::string> getClosestTimestamp();

        double computeCollisionProbability(const SatelliteState &s1, const SatelliteState &s2);

        std::map<int, std::vector<std::pair<int, double>>> computeAllProbabilities(const std::map<int, SatelliteState> &states);

        void storeTopThree(const std::map<int, std::vector<std::pair<int, double>>> &results);
    };
}
