#include "connection_pool.h"

namespace SOEP {
	ConnectionPool& ConnectionPool::getInstance() {
		static ConnectionPool instance;
		return instance;
	}

	void ConnectionPool::initialize(const std::string& connStr, size_t poolSize) {
		std::lock_guard<std::mutex> lock(poolMutex);
		SOEP_ASSERT(!isInitialized, "connection pool is already initialized");

		connString = connStr;
		maxPoolSize = poolSize;

		for (size_t i = 0; i < maxPoolSize; ++i) {
			auto conn = std::make_shared<DatabaseConnection>(connString);
			pool.push(conn);
		}
		isInitialized = true;
	}

	void ConnectionPool::shutdown() {
		std::lock_guard<std::mutex> lock(poolMutex);
		isShuttingDown = true;
		while (!pool.empty()) {
			auto conn = pool.front();
			pool.pop();
			conn->close();
		}
		isInitialized = false;
		isShuttingDown = false;
	}

	std::shared_ptr<DatabaseConnection> ConnectionPool::acquire(int timeoutMs) {
		std::unique_lock<std::mutex> lock(poolMutex);
		SOEP_ASSERT(isInitialized, "connection pool is not initialized");

		bool acquired = poolCondition.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() { return !pool.empty() || isShuttingDown; });

		if (!acquired) {
			spdlog::warn("timeout: failed to acquire db connection");
			return nullptr;
		}

		if (isShuttingDown) {
			return nullptr;
		}

		auto conn = pool.front();
		pool.pop();
		return conn;
	}

	void ConnectionPool::release(std::shared_ptr<DatabaseConnection> conn) {
		std::lock_guard<std::mutex> lock(poolMutex);
		if (conn && conn->isOpen()) {
			pool.push(conn);
			poolCondition.notify_one();
		}
		else {
			spdlog::warn("attempted to release a null or closed connection back to the pool");
		}
	}
}