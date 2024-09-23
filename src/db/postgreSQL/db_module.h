#pragma once
#include "database.h"
#include <pqxx/pqxx>

class PostgreSQL : public Database {
public:
    PostgreSQL();
    ~PostgreSQL();

    bool connect(const std::string& connectionString) override;
    void disconnect() override;
    std::vector<std::string> executeQuery(const std::string& query) override;

private:
    std::unique_ptr<pqxx::connection> connection;
};