/*#include "database_connection.h"

void DatabaseConnection::prepareStmt(const std::string& name, const std::string& query) {
    std::lock_guard<std::mutex> lock(connMutex);
    if (!isOpen()) {
        throw std::runtime_error("connection is not open");
    }
    try {
        conn->prepare(name, query);
        spdlog::info("prepared statement '{}' with query: {}", name, query);
    } catch (const pqxx::sql_error& e) {
        spdlog::error("SQL error during prepare: {}\nQuery: {}", e.what(), e.query());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("error preparing statement '{}': {}", name, e.what());
        throw;
    }
}

std::vector<std::map<std::string, std::string>> DatabaseConnection::executePreparedSelect(const std::string& name, const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lock(connMutex);
    if (!isOpen()) {
        throw std::runtime_error("connection is not open");
    }
    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec_prepared(name, params.begin(), params.end());
        txn.commit();

        std::vector<std::map<std::string, std::string>> results;
        for (const auto& row : res) {
            std::map<std::string, std::string> resultRow;
            for (const auto& field : row) {
                resultRow[field.name()] = field.c_str();
            }
            results.push_back(resultRow);
        }
        return results;
    } catch (const pqxx::sql_error& e) {
        spdlog::error("SQL error during prepared select: {}\nQuery: {}", e.what(), e.query());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("error during prepared select: {}", e.what());
        throw;
    }
}

int DatabaseConnection::executePreparedUpdate(const std::string& name, const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lock(connMutex);
    if (!isOpen()) {
        throw std::runtime_error("connection is not open");
    }
    try {
        if (currentTransaction) {
            pqxx::result res = currentTransaction->exec_prepared(name, params.begin(), params.end());
            return res.affected_rows();
        } else {
            pqxx::work txn(*conn);
            pqxx::result res = txn.exec_prepared(name, params.begin(), params.end());
            int affectedRows = res.affected_rows();
            txn.commit();
            return affectedRows;
        }
    } catch (const pqxx::sql_error& e) {
        spdlog::error("SQL error during prepared update: {}\nQuery: {}", e.what(), e.query());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("error during prepared update: {}", e.what());
        throw;
    }
}*/