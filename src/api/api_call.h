// api/api_call.h
#pragma once

// Function to fetch satellite data
std::string fetchSatelliteData(const std::string& apiKey, int satID, double observer_lat, double observer_lng, double observer_alt, int seconds);
