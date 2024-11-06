#include "database_connection.h"

namespace SOEP {
	DbResponse<void> DatabaseConnection::beginTransaction() {
		SOEP_ASSERT(isOpen(), "connection is not open");
		SOEP_ASSERT(!currentTransaction, "transaction already in progress");

		DbResponse<void> response;
		try {
			currentTransaction = std::make_unique<pqxx::work>(*conn);
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
		SOEP_ASSERT(currentTransaction, "no transaction in progress");

		DbResponse<void> response;
		try {
			currentTransaction->commit();
			currentTransaction.reset();
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
		SOEP_ASSERT(currentTransaction, "no transaction in progress");

		DbResponse<void> response;
		try {
			currentTransaction->abort();
			currentTransaction.reset();
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
}