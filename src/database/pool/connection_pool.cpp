#include "connection_pool.h"

namespace SOEP {
	ConnectionPool& ConnectionPool::getInstance() {
		static ConnectionPool instance;
		return instance;
	}

	void ConnectionPool::initialize(const std::string& connStr, size_t poolSize) {
		std::lock_guard<std::mutex> lock(m_PoolMutex);
		SOEP_ASSERT(!isInitialized, "connection pool is already initialized");

		m_ConnString = connStr;
		m_MaxPoolSize = poolSize;

		for (size_t i = 0; i < m_MaxPoolSize; ++i) {
			auto conn = std::make_unique<DatabaseConnection>(m_ConnString);
			m_Pool.push(std::move(conn));
		}
		m_IsInitialized = true;
	}

	void ConnectionPool::shutdown() {
		std::lock_guard<std::mutex> lock(m_PoolMutex);
		m_IsShuttingDown = true;
		while (!m_Pool.empty()) {
			auto conn = std::move(m_Pool.front());
			m_Pool.pop();
			conn->close();
		}
		m_IsInitialized = false;
		m_IsShuttingDown = false;
	}

	std::unique_ptr<DatabaseConnection> ConnectionPool::acquire(int timeoutMs) {
		std::unique_lock<std::mutex> lock(m_PoolMutex);
		SOEP_ASSERT(isInitialized, "connection pool is not initialized");

		bool acquired = m_PoolCondition.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() { return !m_Pool.empty() || m_IsShuttingDown; });

		if (!acquired) {
			spdlog::warn("timeout: failed to acquire db connection");
			return nullptr;
		}

		if (m_IsShuttingDown) {
			return nullptr;
		}

		auto conn = std::move(m_Pool.front());
		m_Pool.pop();
		return conn;
	}

	void ConnectionPool::release(std::unique_ptr<DatabaseConnection> conn) {
		std::lock_guard<std::mutex> lock(m_PoolMutex);
		if (conn && conn->isOpen()) {
			m_Pool.push(std::move(conn));
			m_PoolCondition.notify_one();
		}
		else {
			spdlog::warn("attempted to release a null or closed connection back to the pool");
		}
	}
}