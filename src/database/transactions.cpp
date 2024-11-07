#include "database_connection.h"

namespace SOEP {
	DbResponse<void> DatabaseConnection::beginTransaction() {
		SOEP_ASSERT(isOpen(), "connection is not open");
		SOEP_ASSERT(!m_CurrentTransaction, "transaction already in progress");

		DbResponse<void> response;
		try {
			m_CurrentTransaction = std::make_unique<pqxx::work>(*m_Conn);
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
		SOEP_ASSERT(m_CurrentTransaction, "no transaction in progress");

		DbResponse<void> response;
		try {
			m_CurrentTransaction->commit();
			m_CurrentTransaction.reset();
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
		SOEP_ASSERT(m_CurrentTransaction, "no transaction in progress");

		DbResponse<void> response;
		try {
			m_CurrentTransaction->abort();
			m_CurrentTransaction.reset();
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