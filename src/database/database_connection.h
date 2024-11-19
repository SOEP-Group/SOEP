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

	template<>
	struct DbResponse<void> {
		bool success;
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

		DbResponse<void> beginTransaction();
		DbResponse<void> commitTransaction();
		DbResponse<void> rollbackTransaction();

		DbResponse<std::vector<std::map<std::string, std::string>>> executeSelectQuery(const std::string& query); // SELECT

		/*
			use this if subject to user input
		*/
		template<typename... Params>
		DbResponse<std::vector<std::map<std::string, std::string>>> executeSelectQuery(const std::string& query, Params&&... params) {
			SOEP_ASSERT(m_Conn && m_Conn->is_open(), "connection is not open");

			DbResponse<std::vector<std::map<std::string, std::string>>> response;
			pqxx::result res;
			try {
				if (m_CurrentTransaction) {
					res = m_CurrentTransaction->exec_params(query, std::forward<Params>(params)...);
				}
				else {
					pqxx::work txn(*m_Conn);
					res = txn.exec_params(query, std::forward<Params>(params)...);
					txn.commit();
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
			SOEP_ASSERT(m_Conn && m_Conn->is_open(), "connection is not open");

			DbResponse<int> response;
			pqxx::result res;
			try {
				if (m_CurrentTransaction) {
					res = m_CurrentTransaction->exec_params(query, std::forward<Params>(params)...);
				}
				else {
					pqxx::work txn(*m_Conn);
					res = txn.exec_params(query, std::forward<Params>(params)...);
					txn.commit();
				}
				response.payload = res.affected_rows();
				response.success = true;
			}
			catch (const pqxx::broken_connection& e) {
				response.success = false;
				response.errorMsg = e.what();
				spdlog::error("Database connection error: {}", e.what());
			}
			catch (const pqxx::sql_error& e) {
				response.success = false;
				response.errorMsg = e.what();
				spdlog::error("SQL error: {}", e.what());
			}

			return response;
		}

		// should never take user input
		DbResponse<void> executeAdminQuery(const std::string& query); // CREATE, ALTER

		void close();

	private:
		void connect();

		std::string m_ConnString;
		std::unique_ptr<pqxx::connection> m_Conn;
		std::unique_ptr<pqxx::work> m_CurrentTransaction;
	};
}
