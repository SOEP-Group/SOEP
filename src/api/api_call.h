// api/api_call.h

#ifndef API_CALL_H
#define API_CALL_H

#include <string>

// Function to fetch satellite data
std::string fetchSatelliteData(const std::string& apiKey, int satID, double observer_lat, double observer_lng, double observer_alt, int seconds);

#endif // API_CALL_H
