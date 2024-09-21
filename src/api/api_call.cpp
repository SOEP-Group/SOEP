// api/api_call.cpp

#include "api_call.h"
#include <curl/curl.h>
#include <iostream>

// Callback function to handle response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string fetchSatelliteData(const std::string& apiKey, int satID, double observer_lat, double observer_lng, double observer_alt, int seconds)
{
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    // Initialize CURL
    curl = curl_easy_init();
    if(curl) {
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
        if(res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            readBuffer.clear(); // Clear the buffer to indicate failure
        }
        // Cleanup
        curl_easy_cleanup(curl);
    }
    return readBuffer;
}
