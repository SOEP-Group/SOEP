#include "pch.h"
#include "database_connection.h"
#include <stdexcept>

namespace SOEP {
	DatabaseConnection::DatabaseConnection(const std::string& connStr)
		: m_ConnString(connStr), m_Conn(nullptr), m_CurrentTransaction(nullptr) {
		connect();
	}

	DatabaseConnection::~DatabaseConnection() {
		close();
	}

	void DatabaseConnection::connect() {
		m_Conn = std::make_unique<pqxx::connection>(m_ConnString);
		if (!m_Conn->is_open()) {
			throw std::runtime_error("failed to open database connection");
		}
		spdlog::info("database connection established");
	}

	bool DatabaseConnection::isOpen() const {
		return m_Conn && m_Conn->is_open();
	}

	void DatabaseConnection::close() {
		if (m_Conn && m_Conn->is_open()) {
			if (m_CurrentTransaction) {
				m_CurrentTransaction->abort();
				m_CurrentTransaction.reset();
			}
			m_Conn.reset();
			spdlog::info("connection closed");
		}
	}

	void DatabaseConnection::getDatabaseVersion() {
		auto res = executeSelectQuery("SELECT version()");
		if (!res.success) {
			spdlog::error("{}", res.errorMsg);
			return;
		}
		for (const auto& row : res.payload) {
			spdlog::info("{}", row.at("version"));
		}
	}

	DbResponse<void> DatabaseConnection::beginTransaction() {
		SOEP_ASSERT(isOpen(), "connection is not open");
		SOEP_ASSERT(!m_CurrentTransaction, "transaction already in progress");

		DbResponse<void> response;
		try {
			m_CurrentTransaction = std::make_unique<pqxx::work>(*m_Conn);
			spdlog::info("transaction started");
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

	DbResponse<void> DatabaseConnection::commitTransaction() {
		SOEP_ASSERT(m_CurrentTransaction, "no transaction in progress");

		DbResponse<void> response;
		try {
			m_CurrentTransaction->commit();
			m_CurrentTransaction.reset();
			spdlog::info("transaction committed");
			response.success = true;
		}
		catch (const pqxx::in_doubt_error& e) {
			response.success = false;
			response.errorMsg = e.what();
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

	DbResponse<void> DatabaseConnection::rollbackTransaction() {
		SOEP_ASSERT(m_CurrentTransaction, "no transaction in progress");

		DbResponse<void> response;
		try {
			m_CurrentTransaction->abort();
			m_CurrentTransaction.reset();
			spdlog::info("transaction rolled back");
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

	DbResponse<std::vector<std::map<std::string, std::string>>> DatabaseConnection::executeSelectQuery(const std::string& query) {
		SOEP_ASSERT(m_Conn && m_Conn->is_open(), "connection is not open");

		DbResponse<std::vector<std::map<std::string, std::string>>> response;
		pqxx::result res;
		try {
			if (m_CurrentTransaction) {
				res = m_CurrentTransaction->exec(query);
				spdlog::info("executed SELECT query in transaction: {}", query);
			}
			else {
				pqxx::work txn(*m_Conn);
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
		
		// unlikely to fail
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
		SOEP_ASSERT(m_Conn && m_Conn->is_open(), "connection is not open");

		DbResponse<int> response;
		pqxx::result res;
		try {
			if (m_CurrentTransaction) {
				res = m_CurrentTransaction->exec(query);
				spdlog::info("executed UPDATE query in transaction: {}", query);
			}
			else {
				pqxx::work txn(*m_Conn);
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
		SOEP_ASSERT(m_Conn && m_Conn->is_open(), "connection is not open");

		DbResponse<void> response;
		try {
			if (m_CurrentTransaction) {
				m_CurrentTransaction->exec(query);
				spdlog::info("executed ADMIN query in transaction: {}", query);
			}
			else {
				pqxx::work txn(*m_Conn);
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