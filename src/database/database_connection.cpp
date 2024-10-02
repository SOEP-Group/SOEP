#include "database_connection.h"
#include <stdexcept>

SOEP::DatabaseConnection::DatabaseConnection(const std::string& connStr)
    : connString(connStr), conn(nullptr), currentTransaction(nullptr) {
    connect();
}

SOEP::DatabaseConnection::~DatabaseConnection() {
    close();
}

void SOEP::DatabaseConnection::connect() {
    conn = std::make_unique<pqxx::connection>(connString);
    if (!conn->is_open()) {
        throw std::runtime_error("failed to open database connection");
    }
    spdlog::info("database connection established");
}

bool SOEP::DatabaseConnection::isOpen() const {
    return conn && conn->is_open();
}

void SOEP::DatabaseConnection::close() {
    if (conn && conn->is_open()) {
        if (currentTransaction) {
            currentTransaction->abort();
            currentTransaction.reset();
        }
        conn.reset();
        spdlog::info("connection closed");
    }
}

void SOEP::DatabaseConnection::getDatabaseVersion() {
    auto result = executeSelectQuery("SELECT version()");
    for (const auto& row : result) {
        spdlog::info("{}", row.at("version"));
    }
}