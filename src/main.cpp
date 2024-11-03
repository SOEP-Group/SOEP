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
            dbConn->executeAdminQuery(
				"CREATE TABLE IF NOT EXISTS satellites ("
				"satellite_id INTEGER PRIMARY KEY, "
				"name TEXT NOT NULL"
				");");
			dbConn->executeAdminQuery(
				"CREATE TABLE IF NOT EXISTS satellite_data ("
				"satellite_id INTEGER NOT NULL REFERENCES satellites(satellite_id), "
				"timestamp TIMESTAMP WITHOUT TIME ZONE NOT NULL, "
				"x_km DOUBLE PRECISION NOT NULL, "
				"y_km DOUBLE PRECISION NOT NULL, "
				"z_km DOUBLE PRECISION NOT NULL, "
				"xdot_km_per_s DOUBLE PRECISION NOT NULL, "
				"ydot_km_per_s DOUBLE PRECISION NOT NULL, "
				"zdot_km_per_s DOUBLE PRECISION NOT NULL, "
				"PRIMARY KEY (satellite_id, timestamp)"
				");");
			dbConn->executeAdminQuery(
				"SELECT create_hypertable('satellite_data', 'timestamp', if_not_exists => TRUE);");

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
