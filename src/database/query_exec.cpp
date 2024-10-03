#include "database_connection.h"

std::vector<std::map<std::string, std::string>> SOEP::DatabaseConnection::executeSelectQuery(const std::string& query) {
    SOEP_ASSERT(conn && conn->is_open(), "connection is not open");

    pqxx::result res;
    try {
        if (currentTransaction) {
            res = currentTransaction->exec(query);
            spdlog::info("executed SELECT query in transaction: {}", query);
        } else {
            pqxx::work txn(*conn);
            res = txn.exec(query);
            txn.commit();
            spdlog::info("executed SELECT query: {}", query);
        }

    } catch (const pqxx::sql_error& e) {
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


int SOEP::DatabaseConnection::executeUpdateQuery(const std::string& query) {
    SOEP_ASSERT(conn && conn->is_open(), "connection is not open");

    pqxx::result res;
    try {
        if (currentTransaction) {
            res = currentTransaction->exec(query);
            spdlog::info("executed UPDATE query in transaction: {}", query);
        } else {
            pqxx::work txn(*conn);
            res = txn.exec(query);
            int affectedRows = res.affected_rows();
            txn.commit();
            spdlog::info("executed UPDATE query: {}", query);
            return affectedRows;
        }
    } catch (const pqxx::sql_error& e) {
        spdlog::error("SQL error: {}\nquery: {}", e.what(), e.query());
        throw;
    }

    return res.affected_rows();
}

/*
    IMPORTANT: dont execute inside threads
*/
void SOEP::DatabaseConnection::executeAdminQuery(const std::string& query) {
    SOEP_ASSERT(conn && conn->is_open(), "connection is not open");

    try {
        if (currentTransaction) {
            currentTransaction->exec(query);
            spdlog::info("executed ADMIN query in transaction: {}", query);
        } else {
            pqxx::work txn(*conn);
            txn.exec(query);
            txn.commit();
            spdlog::info("executed ADMIN query: {}", query);
        }
    } catch (const pqxx::sql_error& e) {
        spdlog::error("SQL error: {}\nquery: {}", e.what(), e.query());
        throw;
    }
}

// test
void SOEP::DatabaseConnection::testQuery() {
    try {
        beginTransaction();

        executeUpdateQuery(
            "INSERT INTO test_table (name, age) VALUES ('Sigma Ben', 50), ('Beta Tom', 1) ON CONFLICT DO NOTHING;"
        );

        auto results = executeSelectQuery("SELECT * FROM test_table");
        for (const auto& row : results) {
            std::string res = "";
            for (const auto& field : row) {
                res += field.first + ": " + field.second + ", ";
            }
            spdlog::info(res);
        }

        commitTransaction();
    } catch (const std::exception& e) {
        rollbackTransaction();
        spdlog::error("error: {}", e.what());
    }
}

// test
void SOEP::DatabaseConnection::test2Query(const std::string& name, const int& age) {
    try {
        beginTransaction();

        executeUpdateQuery(
            "INSERT INTO test_table (name, age) VALUES ($1, $2) ON CONFLICT DO NOTHING;",
            name,
            age
        );

        auto results = executeSelectQuery(
            "SELECT * FROM test_table WHERE name = $1",
            name
        );
        for (const auto& row : results) {
            std::string res = "";
            for (const auto& field : row) {
                res += field.first + ": " + field.second + ", ";
            }
            spdlog::info(res);
        }

        commitTransaction();
    } catch (const std::exception& e) {
        rollbackTransaction();
        spdlog::error("error: {}", e.what());
    }
}