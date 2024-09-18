// src/main.cpp

#include <iostream>
#include <cstdlib> // For getenv()
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

// Callback function for handling the response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

int main()
{
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    // Get API key from environment variable
    const char* apiKeyEnv = std::getenv("N2YO_API_KEY");
    if (apiKeyEnv == nullptr) {
        std::cerr << "Error: N2YO_API_KEY environment variable is not set." << std::endl;
        return 1;
    }
    std::string apiKey(apiKeyEnv);

    // Initialize CURL
    curl = curl_easy_init();
    if(curl) {
        // Satellite ID for ISS
        int satID = 25540;
        // Observer location (latitude, longitude, altitude)
        double observer_lat = 41.702;
        double observer_lng = -76.014;
        double observer_alt = 0;
        // Number of seconds to get positions for
        int seconds = 2;

        // Construct the URL
        std::string url = "https://api.n2yo.com/rest/v1/satellite/positions/"
                          + std::to_string(satID) + "/"
                          + std::to_string(observer_lat) + "/"
                          + std::to_string(observer_lng) + "/"
                          + std::to_string(observer_alt) + "/"
                          + std::to_string(seconds) + "/"
                          + "&apiKey=" + apiKey;

        // Set CURL options
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        // Follow redirects if any
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        // Set up the write callback
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        // Pass the string to the callback function
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        // Perform the request
        res = curl_easy_perform(curl);
        // Check for errors
        if(res != CURLE_OK)
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        else {
            // Parse the JSON response
            auto jsonResponse = json::parse(readBuffer);

            // Output some data
            std::cout << "Satellite Name: " << jsonResponse["info"]["satname"] << std::endl;
            std::cout << "Positions:" << std::endl;

            for (const auto& position : jsonResponse["positions"]) {
                std::cout << "  Timestamp: " << position["timestamp"] << std::endl;
                std::cout << "  Latitude: " << position["satlatitude"] << std::endl;
                std::cout << "  Longitude: " << position["satlongitude"] << std::endl;
                std::cout << "  Altitude: " << position["sataltitude"] << std::endl;
            }
        }

        // Cleanup
        curl_easy_cleanup(curl);
    }
    return 0;
}
