#include "database_connection.h"
#include "../core/assert.h"

void SOEP::DatabaseConnection::beginTransaction() {
    SOEP_ASSERT(isOpen(), "connection is not open");
    SOEP_ASSERT(!currentTransaction, "transaction already in progress");

    currentTransaction = std::make_unique<pqxx::work>(*conn);
    spdlog::info("transaction started");
}

void SOEP::DatabaseConnection::commitTransaction() {
    SOEP_ASSERT(currentTransaction, "no transaction in progress");

    currentTransaction->commit();
    currentTransaction.reset();
    spdlog::info("transaction committed");
}

void SOEP::DatabaseConnection::rollbackTransaction() {
    SOEP_ASSERT(currentTransaction, "no transaction in progress");

    currentTransaction->abort();
    currentTransaction.reset();
    spdlog::info("transaction rolled back");
}