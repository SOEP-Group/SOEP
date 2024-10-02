#pragma once

#include "../database_connection.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

namespace SOEP {
    class ConnectionPool {
    public:
        static ConnectionPool& getInstance();

        void initialize(const std::string& connStr, size_t poolSize);
        void shutdown();

        std::shared_ptr<DatabaseConnection> acquire();
        void release(std::shared_ptr<DatabaseConnection> conn);

    private:
        ConnectionPool() = default;
        ~ConnectionPool() = default;

        // disable copy
        ConnectionPool(const ConnectionPool&) = delete;
        ConnectionPool& operator=(const ConnectionPool&) = delete;

        std::string connString;
        size_t maxPoolSize = 0;
        std::queue<std::shared_ptr<DatabaseConnection>> pool;
        std::mutex poolMutex;
        std::condition_variable poolCondition;
        bool isInitialized = false;
        bool isShuttingDown = false;
    };
}