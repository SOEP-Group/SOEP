#include <pqxx/pqxx>
#include "pch.h"
#include <dotenv/dotenv.h>
#include "core/assert.h"
#include "core/threadpool.h"
#include "network/network.h"
#include "database/database_connection.h"
#include "database/pool/connection_pool.h"


int main()
{
	SOEP_PROFILE_FUNC();
    spdlog::set_level(spdlog::level::trace);
    SOEP::SOEP_SCOPE_TIMER("Main function");
    dotenv::init();
    SOEP::Network::Init();
    SOEP::ThreadPool pool{10};

    const char* dbname = std::getenv("DB_NAME");
    const char* user = std::getenv("DB_USER");
    const char* password = std::getenv("DB_PASSWORD");
    const char* host = std::getenv("DB_HOST");
    const char* port_str = std::getenv("DB_PORT");

    SOEP_ASSERT(dbname != nullptr, "Error: DB_NAME environment variable is not set.");
    SOEP_ASSERT(user != nullptr, "Error: DB_USER environment variable is not set.");
    SOEP_ASSERT(password != nullptr, "Error: DB_PASSWORD environment variable is not set.");
    SOEP_ASSERT(host != nullptr, "Error: DB_HOST environment variable is not set.");
    SOEP_ASSERT(port_str != nullptr, "Error: DB_PORT environment variable is not set.");

    int port = std::stoi(port_str);

	std::string connString = "dbname=" + std::string(dbname) +
							 " user=" + std::string(user) +
							 " password=" + std::string(password) +
							 " host=" + std::string(host) +
							 " port=" + std::to_string(port);

	SOEP::ConnectionPool& connPool = SOEP::ConnectionPool::getInstance();
	connPool.initialize(connString, 10);

	{
		auto dbConn = connPool.acquire();
		if (dbConn) { // change this block later when we have a db schema
			dbConn->getDatabaseVersion();
			dbConn->executeAdminQuery("CREATE TABLE IF NOT EXISTS test_table (id SERIAL PRIMARY KEY, name TEXT, age INT);");
			connPool.release(dbConn);
		}
	}

	// db test
	/*
	for (int i = 0; i < 20; ++i) {
        pool.AddTask([&connPool, i]() {
            auto dbConn = connPool.acquire();
            if (dbConn) {
				dbConn->test2Query("Ben" + std::to_string(i), 20 + i);
                connPool.release(dbConn);
            } else {
                spdlog::error("failed to acquire dbconn");
            }
        });
    }
	*/

	// Get API key from environment variable
	const char *apiKeyEnv = std::getenv("N2YO_API_KEY");
	SOEP_ASSERT(apiKeyEnv != nullptr, "Error: N2YO_API_KEY environment variable is not set.");
	std::string apiKey(apiKeyEnv);

	// Satellite parameters
	int satID = 25540; // As per your code
	double observer_lat = 41.702;
	double observer_lng = -76.014;
	double observer_alt = 0;
	int seconds = 2;

	std::string url = "https://api.n2yo.com/rest/v1/satellite/positions/" + std::to_string(satID) + "/" + std::to_string(observer_lat) + "/" + std::to_string(observer_lng) + "/" + std::to_string(observer_alt) + "/" + std::to_string(seconds) + "/" + "&apiKey=" + apiKeyEnv;

	// Two ways of using the network api. You can pick and choose between them

	// 1st, use call back to recieve and handle the result
	auto promise = pool.AddTask(SOEP::Network::Call, url, [](std::shared_ptr<std::string> result)
								{
									auto jsonResponse = nlohmann::json::parse(result->begin(), result->end()); // We use iterators to avoid copying over the full string to the function
									spdlog::info("Satellite Name: {}", jsonResponse["info"]["satname"].dump());
									spdlog::info("Positions:");
									for (const auto &position : jsonResponse["positions"])
									{
										spdlog::info("Timestamp: {0}", position["timestamp"].dump());
										spdlog::info("Latitude: {0}", position["satlatitude"].dump());
										spdlog::info("Longitude: {0}", position["satlongitude"].dump());
										spdlog::info("Altitude: {0}", position["sataltitude"].dump());
									} }, nullptr, nullptr);

	// Will wait until every task is done, You dont HAVE to run this, this is only important if you want to wait for everything to be done
	// It will freeze the main thread until every task is completed.
	// If syncronization between tasks don't matter, just wait for individual promises
	pool.Await();

	// 2nd, wait for the result to come back like so
	std::shared_ptr<std::string> response = promise.get();

	SOEP_ASSERT(!response->empty(), "Failed to fetch satellite data.");
	// Avoid try catches unless absolutely necessary, they are slow
	auto jsonResponse = nlohmann::json::parse(response->begin(), response->end());

	spdlog::info("Satellite Name: {}", jsonResponse["info"]["satname"].dump());
	spdlog::info("Positions:");
	for (const auto &position : jsonResponse["positions"])
	{
		spdlog::info("Timestamp: {0}", position["timestamp"].dump());
		spdlog::info("Latitude: {0}", position["satlatitude"].dump());
		spdlog::info("Longitude: {0}", position["satlongitude"].dump());
		spdlog::info("Altitude: {0}", position["sataltitude"].dump());
	}

	pool.Await();

    pool.Shutdown();

    connPool.shutdown();

	SOEP::Network::Shutdown();
	SOEP_PROFILE_MARK_END;
	return 0;
}
