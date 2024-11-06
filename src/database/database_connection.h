#pragma once

#include <pqxx/pqxx>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <spdlog/spdlog.h>
#include "core/assert.h"

namespace SOEP {
	template<typename T>
	struct DbResponse {
		bool success;
		T payload;
		std::string errorMsg;
	};

	class DatabaseConnection {
	public:
		explicit DatabaseConnection(const std::string& connStr);
		~DatabaseConnection();

		// disable copy
		DatabaseConnection(const DatabaseConnection&) = delete;
		DatabaseConnection& operator=(const DatabaseConnection&) = delete;

		// enable move
		DatabaseConnection(DatabaseConnection&&) = default;
		DatabaseConnection& operator=(DatabaseConnection&&) = default;

		bool isOpen() const;

		void getDatabaseVersion();

		void testQuery();
		void test2Query(const std::string&, const int&);

		DbResponse<void> beginTransaction();
		DbResponse<void> commitTransaction();
		DbResponse<void> rollbackTransaction();

		DbResponse<std::vector<std::map<std::string, std::string>>> executeSelectQuery(const std::string& query); // SELECT

		/*
			use this if subject to user input
		*/
		template<typename... Params>
		DbResponse<std::vector<std::map<std::string, std::string>>> executeSelectQuery(const std::string& query, Params&&... params) {
			SOEP_ASSERT(conn && conn->is_open(), "connection is not open");

			DbResponse<std::vector<std::map<std::string, std::string>>> response;
			pqxx::result res;
			try {
				if (currentTransaction) {
					res = currentTransaction->exec_params(query, std::forward<Params>(params)...);
					spdlog::info("executed SELECT query in transaction: {}", query);
				}
				else {
					pqxx::work txn(*conn);
					res = txn.exec_params(query, std::forward<Params>(params)...);
					txn.commit();
					spdlog::info("executed SELECT query {}", query);
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

		DbResponse<int> executeUpdateQuery(const std::string& query); // INSERT, UPDATE, DELETE

		/*
			use this if subject to user input
		*/
		template<typename... Params>
		DbResponse<int> executeUpdateQuery(const std::string& query, Params&&... params) {
			SOEP_ASSERT(conn && conn->is_open(), "connection is not open");

			DbResponse<int> response;
			pqxx::result res;
			try {
				if (currentTransaction) {
					res = currentTransaction->exec_params(query, std::forward<Params>(params)...);
					spdlog::info("executed UPDATE query in transaction: {}", query);
				}
				else {
					pqxx::work txn(*conn);
					res = txn.exec_params(query, std::forward<Params>(params)...);
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

		// should never take user input
		DbResponse<void> executeAdminQuery(const std::string& query); // CREATE, ALTER

		void close();

	private:
		void connect();

		std::string connString;
		std::unique_ptr<pqxx::connection> conn;
		std::unique_ptr<pqxx::work> currentTransaction;
	};
}
