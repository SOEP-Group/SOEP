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

}