#include "database_connection.h"
#include "pool/connection_pool.h"

// RAII wrapper for database connections
namespace SOEP {
    class ScopedConnection {
    public:
        ScopedConnection(ConnectionPool& pool) : m_Pool(pool), m_DbConn(pool.acquire()) {}
        ~ScopedConnection() {
            if (m_DbConn) {
                m_Pool.release(std::move(m_DbConn));
            }
        }

        DatabaseConnection* operator->() {
            return m_DbConn.get();
        }

        DatabaseConnection& operator*() {
            return *m_DbConn;
        }

        operator bool() const {
            return m_DbConn != nullptr;
        }

        ScopedConnection(const ScopedConnection&) = delete;
        ScopedConnection& operator=(const ScopedConnection&) = delete;

        ScopedConnection(ScopedConnection&& other) noexcept : m_Pool(other.m_Pool), m_DbConn(std::move(other.m_DbConn)) {}

        ScopedConnection& operator=(ScopedConnection&& other) noexcept {
            if (this != &other) {
                m_DbConn = std::move(other.m_DbConn);
            }
            return *this;
        }
    private:
        ConnectionPool& m_Pool;
        std::unique_ptr<DatabaseConnection> m_DbConn;
    };
}