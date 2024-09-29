#include "database_connection.h"
#include <stdexcept>

DatabaseConnection::DatabaseConnection(const std::string& connStr)
    : connString(connStr), conn(nullptr), currentTransaction(nullptr) {
    connect();
}

DatabaseConnection::~DatabaseConnection() {
    close();
}

void DatabaseConnection::connect() {
    std::lock_guard<std::mutex> lock(connMutex);
    try {
        conn = std::make_unique<pqxx::connection>(connString);
        if (!conn->is_open()) {
            throw std::runtime_error("failed to open database connection");
        }
        spdlog::info("database connection established");
    } catch (const std::exception& e) {
        spdlog::error("connection error: {}", e.what());
        throw;
    }
}

bool DatabaseConnection::isOpen() const {
    std::lock_guard<std::mutex> lock(connMutex);
    return conn && conn->is_open();
}

void DatabaseConnection::close() {
    std::lock_guard<std::mutex> lock(connMutex);
    if (conn && conn->is_open()) {
        if (currentTransaction) {
            currentTransaction->abort();
            currentTransaction.reset();
        }
        //conn->close();
        conn.reset();
        spdlog::info("connection closed");
    }
}

void DatabaseConnection::getDatabaseVersion() {
    auto result = executeSelectQuery("SELECT version()");
    for (const auto& row : result) {
        spdlog::info("{}", row.at("version"));
    }
}