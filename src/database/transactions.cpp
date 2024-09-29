#include "database_connection.h"

void DatabaseConnection::beginTransaction() {
    std::lock_guard<std::mutex> lock(connMutex);
    if (!isOpen()) {
        throw std::runtime_error("connection is not open");
    }
    if (currentTransaction) {
        throw std::runtime_error("transaction already in progress");
    }
    currentTransaction = std::make_unique<pqxx::work>(*conn);
    spdlog::info("transaction started");
}

void DatabaseConnection::commitTransaction() {
    std::lock_guard<std::mutex> lock(connMutex);
    if (currentTransaction) {
        currentTransaction->commit();
        currentTransaction.reset();
        spdlog::info("transaction committed");
    } else {
        throw std::runtime_error("no transaction in progress");
    }
}

void DatabaseConnection::rollbackTransaction() {
    std::lock_guard<std::mutex> lock(connMutex);
    if (currentTransaction) {
        currentTransaction->abort();
        currentTransaction.reset();
        spdlog::info("transaction rolled back");
    } else {
        throw std::runtime_error("no transaction in progress");
    }
}
