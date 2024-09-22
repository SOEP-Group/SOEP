// src/main.cpp
#include "pch.h"
#include <dotenv/dotenv.h>
#include "api/api_call.h"
#include "core/assert.h"
#include "core/threadpool.h"

int main()
{
	dotenv::init();
	SOEP::ThreadPool pool{ 10 };
	// Get API key from environment variable
	const char* apiKeyEnv = std::getenv("N2YO_API_KEY");
	SOEP_ASSERT(apiKeyEnv != nullptr, "Error: N2YO_API_KEY environment variable is not set.");
	std::string apiKey(apiKeyEnv);

	// Satellite parameters
	int satID = 25540; // As per your code
	double observer_lat = 41.702;
	double observer_lng = -76.014;
	double observer_alt = 0;
	int seconds = 2;

	auto promise = pool.AddTask(fetchSatelliteData, apiKey, satID, observer_lat, observer_lng, observer_alt, seconds);

	// Will wait until every task is done, You dont HAVE to run this, this is only important if you want to wait for everything to be done
	// It will freeze the main thread until every task is completed.
	// If syncronization between tasks don't matter, just wait for individual promises
	pool.Await();

	std::string response = promise.get();

	SOEP_ASSERT(!response.empty(), "Failed to fetch satellite data.");
	// Avoid try catches unless absolutely necessary, they are slow
	auto jsonResponse = nlohmann::json::parse(response);

	spdlog::info("Satellite Name: {}", jsonResponse["info"]["satname"].dump());
	spdlog::info("Positions:");
	for (const auto& position : jsonResponse["positions"]) {
		spdlog::info("Timestamp: {0}", position["timestamp"].dump());
		spdlog::info("Latitude: {0}", position["satlatitude"].dump());
		spdlog::info("Longitude: {0}", position["satlongitude"].dump());
		spdlog::info("Altitude: {0}", position["sataltitude"].dump());
	}


	return 0;
}
