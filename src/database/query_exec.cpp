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
}