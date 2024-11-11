#pragma once

#include "database/database_connection.h"
#include <queue>
#include <mutex>
#include <condition_variable>

namespace SOEP {
	class ConnectionPool {
	public:
		static ConnectionPool& getInstance();

		void initialize(const std::string& connStr, size_t poolSize);
		void shutdown();

		std::unique_ptr<DatabaseConnection> acquire(int timeoutMs = 1000);
		void release(std::unique_ptr<DatabaseConnection> conn);

	private:
		ConnectionPool() = default;
		~ConnectionPool() = default;

		// disable copy
		ConnectionPool(const ConnectionPool&) = delete;
		ConnectionPool& operator=(const ConnectionPool&) = delete;

		std::string m_ConnString;
		size_t m_MaxPoolSize = 0;
		std::queue<std::unique_ptr<DatabaseConnection>> m_Pool;
		std::mutex m_PoolMutex;
		std::condition_variable m_PoolCondition;
		bool m_IsInitialized = false;
		bool m_IsShuttingDown = false;
	};
}