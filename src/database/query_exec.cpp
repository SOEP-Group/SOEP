#include "database_connection.h"

std::vector<std::map<std::string, std::string>> DatabaseConnection::executeSelectQuery(const std::string& query) {
    std::lock_guard<std::mutex> lock(connMutex);
    if (!conn || !conn->is_open()) {
        throw std::runtime_error("connection is not open");
    }
    try {
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec(query);
        txn.commit();
        spdlog::info("executed SELECT query: {}", query);

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
        spdlog::error("SQL error: {}\nquery: {}", e.what(), e.query());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("error executing SELECT query: {}", e.what());
        throw;
    }
}

int DatabaseConnection::executeUpdateQuery(const std::string& query) {
    std::lock_guard<std::mutex> lock(connMutex);
    if (!conn || !conn->is_open()) {
        throw std::runtime_error("connection is not open");
    }
    try {
        if (currentTransaction) {
            pqxx::result res = currentTransaction->exec(query);
            spdlog::info("executed UPDATE query in transaction: {}", query);
            return res.affected_rows();
        } else {
            pqxx::work txn(*conn);
            pqxx::result res = txn.exec(query);
            int affectedRows = res.affected_rows();
            txn.commit();
            spdlog::info("executed UPDATE query: {}", query);
            return affectedRows;
        }
    } catch (const pqxx::sql_error& e) {
        spdlog::error("SQL error: {}\nquery: {}", e.what(), e.query());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("error executing UPDATE query: {}", e.what());
        throw;
    }
}

void DatabaseConnection::executeAdminQuery(const std::string& query) {
    std::lock_guard<std::mutex> lock(connMutex);
    if (!conn || !conn->is_open()) {
        throw std::runtime_error("connection is not open");
    }
    try {
        if (currentTransaction) {
            currentTransaction->exec(query);
        } else {
            pqxx::work txn(*conn);
            txn.exec(query);
            txn.commit();
        }
    } catch (const pqxx::sql_error& e) {
        spdlog::error("SQL error: {}\nquery: {}", e.what(), e.query());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("error: {}", e.what());
        throw;
    }
}

// test
#include <iostream>
void DatabaseConnection::testQuery() {
    try {
        beginTransaction();

        executeAdminQuery("CREATE TABLE IF NOT EXISTS test_table (id SERIAL PRIMARY KEY, name TEXT, age INT);");

        executeUpdateQuery(
            "INSERT INTO test_table (name, age) VALUES "
            "('Sigma Ben', 50), ('Beta Tom', 1) "
            "ON CONFLICT DO NOTHING;");

        auto results = executeSelectQuery("SELECT * FROM test_table");
        for (const auto& row : results) {
            for (const auto& field : row) {
                spdlog::info("{}: {}", field.first, field.second);
            }
        }

        commitTransaction();
        spdlog::debug("transaction committed");
    } catch (const std::exception& e) {
        rollbackTransaction();
        spdlog::error("transaction rollbacked");
        spdlog::error("error: {}", e.what());
    }
}
