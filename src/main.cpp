#include "pch.h"
#include <dotenv/dotenv.h>
#include "core/assert.h"
#include "core/threadpool.h"
#include "network/network.h"
#include "database/database_connection.h"
#include "database/pool/connection_pool.h"
#include "satellite_processor.h"

int main()
{
	SOEP_PROFILE_FUNC();
	spdlog::set_level(spdlog::level::trace);
	SOEP::SOEP_SCOPE_TIMER("Main function"); // A timer that will end once scope has ended (a scope is everything within a block of {})
	dotenv::init();
	SOEP::Network::Init();

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
		if (dbConn) {
			dbConn->getDatabaseVersion();
			// assuming our db schema will look something like this, a table for a satellite
			dbConn->executeAdminQuery(
				"CREATE TABLE IF NOT EXISTS satellite_data("
				"id SERIAL PRIMARY KEY, "
				"satellite_id INT NOT NULL, "
				"tsince_min DOUBLE PRECISION, "
				"x_km DOUBLE PRECISION, "
				"y_km DOUBLE PRECISION, "
				"z_km DOUBLE PRECISION, "
				"xdot_km_per_s DOUBLE PRECISION, "
				"ydot_km_per_s DOUBLE PRECISION, "
				"zdot_km_per_s DOUBLE PRECISION"
				");"
			);
			connPool.release(dbConn);
		}
	}

	// Get API key from environment variable
	const char* apiKeyEnv = std::getenv("N2YO_API_KEY");
	SOEP_ASSERT(apiKeyEnv != nullptr, "Error: N2YO_API_KEY environment variable is not set.");
	std::string apiKey(apiKeyEnv);

	SOEP::SatelliteProcessor processor(apiKey, 10);
	processor.invoke();

	connPool.shutdown();
	SOEP::Network::Shutdown();
	SOEP_PROFILE_MARK_END;

	return 0;
}
