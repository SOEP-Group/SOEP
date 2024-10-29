#include "database/database_connection.h"
#include "database/pool/connection_pool.h"

namespace SOEP {
    class ScopedDbConn {
    public:
        ScopedDbConn(ConnectionPool& pool) : m_Pool(pool), m_DbConn(pool.acquire()) {}
        ~ScopedDbConn() {
            if (m_DbConn) {
                m_Pool.release(m_DbConn);
            }
        }
        std::shared_ptr<DatabaseConnection> get() { return m_DbConn; }

    private:
        ConnectionPool& m_Pool;
        std::shared_ptr<DatabaseConnection> m_DbConn;
    };
}