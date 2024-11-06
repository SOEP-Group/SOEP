#include "pch.h"
#include "database_connection.h"
#include <stdexcept>

namespace SOEP {
	DatabaseConnection::DatabaseConnection(const std::string& connStr)
		: connString(connStr), conn(nullptr), currentTransaction(nullptr) {
		connect();
	}

	DatabaseConnection::~DatabaseConnection() {
		close();
	}

	void DatabaseConnection::connect() {
		conn = std::make_unique<pqxx::connection>(connString);
		if (!conn->is_open()) {
			throw std::runtime_error("failed to open database connection");
		}
		spdlog::info("database connection established");
	}

	bool DatabaseConnection::isOpen() const {
		return conn && conn->is_open();
	}

	void DatabaseConnection::close() {
		if (conn && conn->is_open()) {
			if (currentTransaction) {
				currentTransaction->abort();
				currentTransaction.reset();
			}
			conn.reset();
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