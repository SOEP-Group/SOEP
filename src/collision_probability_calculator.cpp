// collision_probability_calculator.cpp

#include "collision_probability_calculator.h"
#include "database/pool/connection_pool.h"
#include "database/scoped_connection.h"
#include "core/threadpool.h"
#include <cmath>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <mutex>
#include <future>

namespace SOEP
{
    static void invert2x2(double a, double b, double c, double d, double &det, double &A, double &B, double &C, double &D)
    {
        det = a * d - b * c;
        if (std::fabs(det) < 1e-15)
        {
            det = 1e-15;
        }
        double invDet = 1.0 / det;
        A = d * invDet;
        B = -b * invDet;
        C = -c * invDet;
        D = a * invDet;
    }

    static double gaussian2D(double x, double y,
                             double meanX, double meanY,
                             double a, double b, double d)
    {
        double c = b; // covariance symmetric
        double det, A, B, C, D;
        invert2x2(a, b, c, d, det, A, B, C, D);

        double dx = x - meanX;
        double dy = y - meanY;
        double exponent = -0.5 * (A * dx * dx + 2 * B * dx * dy + D * dy * dy);

        double norm = 1.0 / (2.0 * M_PI * std::sqrt(det));
        return norm * std::exp(exponent);
    }

    static double integrateGaussianOverCircle(double meanX, double meanY,
                                              double a, double b, double d,
                                              double radius)
    {
        int N_radial = 100;  // can adjust for accuracy/performance
        int N_angular = 100; // can adjust for accuracy/performance
        double dtheta = 2.0 * M_PI / N_angular;
        double dr = radius / N_radial;
        double sum = 0.0;
        for (int i = 0; i < N_angular; i++)
        {
            double theta = i * dtheta;
            double cosT = std::cos(theta);
            double sinT = std::sin(theta);
            for (int j = 0; j < N_radial; j++)
            {
                double r = (j + 0.5) * dr; // midpoint rule
                double x = r * cosT;
                double y = r * sinT;
                double val = gaussian2D(x, y, meanX, meanY, a, b, d);
                sum += val * r * dr * dtheta;
            }
        }
        return sum;
    }

    // Constructor
    CollisionProbabilityCalculator::CollisionProbabilityCalculator() {}

    // Destructor defined as default
    CollisionProbabilityCalculator::~CollisionProbabilityCalculator() = default;

    std::map<int, SatelliteState> CollisionProbabilityCalculator::retrieveSatelliteStates(const std::string &timestamp)
    {
        auto &connPool = ConnectionPool::getInstance();
        ScopedConnection conn(connPool);
        std::map<int, SatelliteState> states;

        if (!conn)
        {
            spdlog::error("DB connection not available");
            return states;
        }

        std::string query = "SELECT satellite_id, x_km, y_km, z_km, xdot_km_per_s, ydot_km_per_s, zdot_km_per_s "
                            "FROM orbit_data ;";

        auto response = conn->executeSelectQuery(query);
        if (!response.success)
        {
            spdlog::error("Error fetching satellite states: {}", response.errorMsg);
            return states;
        }

        for (auto &row : response.payload)
        {
            SatelliteState s;
            s.satellite_id = std::stoi(row.at("satellite_id"));
            s.x_km = std::stod(row.at("x_km"));
            s.y_km = std::stod(row.at("y_km"));
            s.z_km = std::stod(row.at("z_km"));
            s.vx_km_s = std::stod(row.at("xdot_km_per_s"));
            s.vy_km_s = std::stod(row.at("ydot_km_per_s"));
            s.vz_km_s = std::stod(row.at("zdot_km_per_s"));

            states[s.satellite_id] = s;
        }

        return states;
    }

    double CollisionProbabilityCalculator::computeCollisionProbability(const SatelliteState &s1, const SatelliteState &s2)
    {
        double dx = (s2.x_km - s1.x_km) * 1000.0;
        double dy = (s2.y_km - s1.y_km) * 1000.0;

        double radius = 10.0;
        double a = 10000.0;
        double b = 0.0;
        double d = 10000.0;

        double probability = integrateGaussianOverCircle(dx, dy, a, b, d, radius);
        return probability;
    }

    std::optional<std::string> CollisionProbabilityCalculator::getClosestTimestamp()
    {
        auto &connPool = ConnectionPool::getInstance();
        ScopedConnection conn(connPool);
        if (!conn)
        {
            spdlog::error("DB connection not available for fetching closest timestamp.");
            return std::nullopt;
        }

        std::string query = "SELECT timestamp FROM orbit_data "
                            "ORDER BY ABS(EXTRACT(EPOCH FROM timestamp - NOW())) ASC "
                            "LIMIT 1;";

        auto response = conn->executeSelectQuery(query);
        if (!response.success)
        {
            spdlog::error("Error fetching closest timestamp: {}", response.errorMsg);
            return std::nullopt;
        }

        if (response.payload.empty())
        {
            spdlog::error("No timestamps found in orbit_data.");
            return std::nullopt;
        }

        std::string closest_timestamp = response.payload[0]["timestamp"];
        return closest_timestamp;
    }

    std::map<int, std::vector<std::pair<int, double>>> CollisionProbabilityCalculator::computeAllProbabilities(const std::map<int, SatelliteState> &states)
    {
        std::map<int, std::vector<std::pair<int, double>>> results;
        std::vector<int> sat_ids;
        sat_ids.reserve(states.size());
        for (auto &kv : states)
        {
            sat_ids.push_back(kv.first);
        }

        std::mutex resultsMutex;

        SOEP::ThreadPool pool{30};

        std::vector<std::future<void>> futures;
        futures.reserve(sat_ids.size());

        // Parallelize the O(N²) loop
        for (size_t i = 0; i < sat_ids.size(); i++)
        {
            int s1_id = sat_ids[i];
            futures.push_back(pool.AddTask([&, s1_id, i]()
                                           {
                std::vector<std::pair<int, double>> localResults;
                localResults.reserve(sat_ids.size() - i - 1);

                if (i % 100 == 0) // Adjust the modulus as needed
                {
                    spdlog::info("Processing satellite {}/{}", i + 1, sat_ids.size());
                }


                for (size_t j = i + 1; j < sat_ids.size(); j++)
                {
                    int s2_id = sat_ids[j];
                    double p = computeCollisionProbability(states.at(s1_id), states.at(s2_id));
                    localResults.emplace_back(s2_id, p);
                }

                // Merge into global results
                {
                    std::lock_guard<std::mutex> lock(resultsMutex);
                    auto &vecForS1 = results[s1_id];
                    vecForS1.insert(vecForS1.end(), localResults.begin(), localResults.end());

                    for (auto &pr : localResults)
                    {
                        results[pr.first].push_back({s1_id, pr.second});
                    }
                } }));
        }

        // Wait for all tasks
        for (auto &f : futures)
        {
            f.get();
        }

        pool.Await();
        pool.Shutdown();

        return results;
    }

    void CollisionProbabilityCalculator::storeTopThree(const std::map<int, std::vector<std::pair<int, double>>> &results)
    {
        auto &connPool = ConnectionPool::getInstance();
        ScopedConnection conn(connPool);
        if (!conn)
        {
            spdlog::error("No DB connection available to store top 3 results");
            return;
        }

        // Start transaction
        auto beginResponse = conn->beginTransaction();
        if (!beginResponse.success)
        {
            spdlog::error("Failed to start transaction for storing top 3 probabilities: {}", beginResponse.errorMsg);
            return;
        }

        // Prepare for batch insertion
        std::ostringstream insertStream;
        insertStream << "INSERT INTO top_collision_probabilities "
                        "(satellite_id, rank, other_satellite_id, probability, calculation_time) VALUES ";

        bool firstValue = true;
        const int batchSize = 1000; // Adjust based on memory and performance
        int currentBatchSize = 0;
        int totalInserts = 0;

        // Use a single NOW() call for all inserts in this batch
        // This ensures consistency in calculation_time across the batch
        std::string calculation_time = "NOW()";

        for (const auto &kv : results)
        {
            int sat_id = kv.first;
            const auto &vec = kv.second;

            // Sort in descending order of probability
            std::vector<std::pair<int, double>> sortedVec = vec;
            std::sort(sortedVec.begin(), sortedVec.end(), [](const std::pair<int, double> &a, const std::pair<int, double> &b)
                      { return a.second > b.second; });

            // Take top three
            int topN = std::min(3, static_cast<int>(sortedVec.size()));
            for (int rank = 1; rank <= topN; ++rank)
            {
                int other_id = sortedVec[rank - 1].first;
                double prob = sortedVec[rank - 1].second;

                // Append to the batch
                if (!firstValue)
                {
                    insertStream << ", ";
                }
                else
                {
                    firstValue = false;
                }

                insertStream << "("
                             << sat_id << ", "
                             << rank << ", "
                             << other_id << ", "
                             << prob << ", "
                             << calculation_time << ")";

                currentBatchSize++;
                totalInserts++;

                // If batch size is reached, execute the batch
                if (currentBatchSize >= batchSize)
                {
                    insertStream << " ON CONFLICT (satellite_id, rank) DO UPDATE SET "
                                 << "other_satellite_id = EXCLUDED.other_satellite_id, "
                                 << "probability = EXCLUDED.probability, "
                                 << "calculation_time = EXCLUDED.calculation_time;";

                    auto updateResponse = conn->executeUpdateQuery(insertStream.str());
                    if (!updateResponse.success)
                    {
                        spdlog::error("Batch insert query failed: {}", updateResponse.errorMsg);
                        conn->rollbackTransaction();
                        return;
                    }

                    // Reset for next batch
                    insertStream.str("");
                    insertStream.clear();
                    insertStream << "INSERT INTO top_collision_probabilities "
                                    "(satellite_id, rank, other_satellite_id, probability, calculation_time) VALUES ";
                    firstValue = true;
                    currentBatchSize = 0;

                    // Optional: Log progress
                    spdlog::info("Inserted {} records so far...", totalInserts);
                }
            }
        }

        // Insert any remaining records
        if (currentBatchSize > 0)
        {
            insertStream << " ON CONFLICT (satellite_id, rank) DO UPDATE SET "
                         << "other_satellite_id = EXCLUDED.other_satellite_id, "
                         << "probability = EXCLUDED.probability, "
                         << "calculation_time = EXCLUDED.calculation_time;";

            auto updateResponse = conn->executeUpdateQuery(insertStream.str());
            if (!updateResponse.success)
            {
                spdlog::error("Final batch insert query failed: {}", updateResponse.errorMsg);
                conn->rollbackTransaction();
                return;
            }
        }

        // Commit transaction
        auto commitResponse = conn->commitTransaction();
        if (!commitResponse.success)
        {
            spdlog::error("Failed to commit top 3 collision probabilities transaction: {}", commitResponse.errorMsg);
            conn->rollbackTransaction();
        }
        else
        {
            spdlog::info("Top 3 collision probabilities per satellite stored successfully. {} rows affected.", totalInserts);
        }
    }
}
