// src/main.cpp

#include <iostream>
#include <cstdlib> // For getenv()
#include <string>
#include <nlohmann/json.hpp>
#include "api/api_call.h"

using json = nlohmann::json;

int main()
{
    // Get API key from environment variable
    const char* apiKeyEnv = std::getenv("N2YO_API_KEY");
    if (apiKeyEnv == nullptr) {
        std::cerr << "Error: N2YO_API_KEY environment variable is not set." << std::endl;
        return 1;
    }
    std::string apiKey(apiKeyEnv);

    // Satellite parameters
    int satID = 25540; // As per your code
    double observer_lat = 41.702;
    double observer_lng = -76.014;
    double observer_alt = 0;
    int seconds = 2;

    // Fetch satellite data
    std::string response = fetchSatelliteData(apiKey, satID, observer_lat, observer_lng, observer_alt, seconds);

    if (!response.empty()) {
        try {
            // Parse the JSON response
            auto jsonResponse = json::parse(response);

            // Output some data
            std::cout << "Satellite Name: " << jsonResponse["info"]["satname"] << std::endl;
            std::cout << "Positions:" << std::endl;

            for (const auto& position : jsonResponse["positions"]) {
                std::cout << "  Timestamp: " << position["timestamp"] << std::endl;
                std::cout << "  Latitude: " << position["satlatitude"] << std::endl;
                std::cout << "  Longitude: " << position["satlongitude"] << std::endl;
                std::cout << "  Altitude: " << position["sataltitude"] << std::endl;
            }
        } catch (const json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "Failed to fetch satellite data." << std::endl;
    }

    return 0;
}
