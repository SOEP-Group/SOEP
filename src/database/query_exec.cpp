#include "database_connection.h"

namespace SOEP {
	DbResponse<std::vector<std::map<std::string, std::string>>> DatabaseConnection::executeSelectQuery(const std::string& query) {
		SOEP_ASSERT(conn && conn->is_open(), "connection is not open");

		DbResponse<std::vector<std::map<std::string, std::string>>> response;
		pqxx::result res;
		try {
			if (currentTransaction) {
				res = currentTransaction->exec(query);
				spdlog::info("executed SELECT query in transaction: {}", query);
			}
			else {
				pqxx::work txn(*conn);
				res = txn.exec(query);
				txn.commit();
				spdlog::info("executed SELECT query: {}", query);
			}
		}
		catch (const pqxx::broken_connection& e) {
			response.success = false;
			response.errorMsg = e.what();
			return response;
		}
		catch (const pqxx::sql_error& e) {
			response.success = false;
			response.errorMsg = e.what();
			return response;
		}
		
		// unlikely to fail, logically, should only fail if severe system issues such we get exception of 'bad_alloc'
		for (const auto& row : res) {
			std::map<std::string, std::string> resultRow;
			for (const auto& field : row) {
				resultRow[field.name()] = field.c_str();
			}
			response.payload.push_back(resultRow);
		}
		response.success = true;

		return response;
	}


	DbResponse<int> DatabaseConnection::executeUpdateQuery(const std::string& query) {
		SOEP_ASSERT(conn && conn->is_open(), "connection is not open");

		DbResponse<int> response;
		pqxx::result res;
		try {
			if (currentTransaction) {
				res = currentTransaction->exec(query);
				spdlog::info("executed UPDATE query in transaction: {}", query);
			}
			else {
				pqxx::work txn(*conn);
				res = txn.exec(query);
				int affectedRows = res.affected_rows();
				txn.commit();
				spdlog::info("executed UPDATE query: {}", query);
			}
			response.payload = res.affected_rows();
			response.success = true;
		}
		catch (const pqxx::broken_connection& e) {
			response.success = false;
			response.errorMsg = e.what();
		}
		catch (const pqxx::sql_error& e) {
			response.success = false;
			response.errorMsg = e.what();
		}

		return response;
	}

	/*
		IMPORTANT: dont execute inside threads
	*/
	DbResponse<void> DatabaseConnection::executeAdminQuery(const std::string& query) {
		SOEP_ASSERT(conn && conn->is_open(), "connection is not open");

		DbResponse<void> response;
		try {
			if (currentTransaction) {
				currentTransaction->exec(query);
				spdlog::info("executed ADMIN query in transaction: {}", query);
			}
			else {
				pqxx::work txn(*conn);
				txn.exec(query);
				txn.commit();
				spdlog::info("executed ADMIN query: {}", query);
			}
			response.success = true;
		}
		catch (const pqxx::broken_connection& e) {
			response.success = false;
			response.errorMsg = e.what();
		}
		catch (const pqxx::sql_error& e) {
			response.success = false;
			response.errorMsg = e.what();
		}

		return response;
	}

	/*
	// test
	void DatabaseConnection::testQuery() {
		try {
			beginTransaction();

			executeUpdateQuery(
				"INSERT INTO test_table (name, age) VALUES ('Sigma Ben', 50), ('Beta Tom', 1) ON CONFLICT DO NOTHING;"
			);

			auto results = executeSelectQuery("SELECT * FROM test_table");
			for (const auto& row : results) {
				std::string res = "";
				for (const auto& field : row) {
					res += field.first + ": " + field.second + ", ";
				}
				spdlog::info(res);
			}

			commitTransaction();
		}
		catch (const std::exception& e) {
			rollbackTransaction();
			spdlog::error("error: {}", e.what());
		}
	}

	// test
	void DatabaseConnection::test2Query(const std::string& name, const int& age) {
		try {
			beginTransaction();

			executeUpdateQuery(
				"INSERT INTO test_table (name, age) VALUES ($1, $2) ON CONFLICT DO NOTHING;",
				name,
				age
			);

			auto results = executeSelectQuery(
				"SELECT * FROM test_table WHERE name = $1",
				name
			);
			for (const auto& row : results) {
				std::string res = "";
				for (const auto& field : row) {
					res += field.first + ": " + field.second + ", ";
				}
				spdlog::info(res);
			}

			commitTransaction();
		}
		catch (const std::exception& e) {
			rollbackTransaction();
			spdlog::error("error: {}", e.what());
		}
	}
	*/
}