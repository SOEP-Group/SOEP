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

		void beginTransaction();
		void commitTransaction();
		void rollbackTransaction();

		DbResponse<std::vector<std::map<std::string, std::string>>> executeSelectQuery(const std::string& query); // SELECT

		/*
			use this if subject to user input
		*/
		template<typename... Params>
		DbResponse<std::vector<std::map<std::string, std::string>>> executeSelectQuery(const std::string& query, Params&&... params) {
			SOEP_ASSERT(conn && conn->is_open(), "connection is not open");

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
			catch (const pqxx::sql_error& e) {
				spdlog::error("SQL error: {}\nquery: {}", e.what(), e.query());
				throw;
			}

			std::vector<std::map<std::string, std::string>> results;
			for (const auto& row : res) {
				std::map<std::string, std::string> resultRow;
				for (const auto& field : row) {
					resultRow[field.name()] = field.c_str();
				}
				results.push_back(resultRow);
			}
			return results;
		}

		DbResponse<int> executeUpdateQuery(const std::string& query); // INSERT, UPDATE, DELETE

		/*
			use this if subject to user input
		*/
		template<typename... Params>
		DbResponse<int> executeUpdateQuery(const std::string& query, Params&&... params) {
			SOEP_ASSERT(conn && conn->is_open(), "connection is not open");

			pqxx::result res;
			try {
				if (currentTransaction) {
					res = currentTransaction->exec_params(query, std::forward<Params>(params)...);
					spdlog::info("executed UPDATE query in transaction: {}", query);
				}
				else {
					pqxx::work txn(*conn);
					res = txn.exec_params(query, std::forward<Params>(params)...);
					int affectedRows = res.affected_rows();
					txn.commit();
					spdlog::info("executed UPDATE query: {}", query);
					return affectedRows;
				}
			}
			catch (const pqxx::sql_error& e) {
				spdlog::error("SQL error: {}\nquery: {}", e.what(), e.query());
				throw;
			}
			return res.affected_rows();
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
