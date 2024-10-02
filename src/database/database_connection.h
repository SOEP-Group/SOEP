#pragma once

#include <pqxx/pqxx>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <spdlog/spdlog.h>

namespace SOEP {
    class DatabaseConnection {
    public:
        explicit DatabaseConnection(const std::string& connStr);
        ~DatabaseConnection();

        // disable copy
        DatabaseConnection(const DatabaseConnection&) = delete;
        DatabaseConnection& operator=(const DatabaseConnection&) = delete;

        // enable move
        DatabaseConnection(DatabaseConnection&&) = default;
        DatabaseConnection& operator=(DatabaseConnection&&) = default;

        bool isOpen() const;

        void getDatabaseVersion();

        void testQuery();
        void test2Query(const std::string&, const int&);

        void beginTransaction();
        void commitTransaction();
        void rollbackTransaction();

        std::vector<std::map<std::string, std::string>> executeSelectQuery(const std::string& query); // SELECT
        int executeUpdateQuery(const std::string& query); // INSERT, UPDATE, DELETE
        void executeAdminQuery(const std::string& query); // CREATE, ALTER

        /*void prepareStmt(const std::string& name, const std::string& query);
        std::vector<std::map<std::string, std::string>> executePreparedSelect(const std::string& name, const std::vector<std::string>& params);
        int executePreparedUpdate(const std::string& name, const std::vector<std::string>& params);*/

        void close();

    private:
        void connect();

        std::string connString;
        std::unique_ptr<pqxx::connection> conn;
        std::unique_ptr<pqxx::work> currentTransaction;
    };
}