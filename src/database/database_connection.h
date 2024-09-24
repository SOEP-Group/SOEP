#ifndef DATABASE_CONNECTION_H
#define DATABASE_CONNECTION_H

#include <pqxx/pqxx>
#include <string>

class DatabaseConnection
{
public:
    DatabaseConnection(const std::string &dbname, const std::string &user, const std::string &password, const std::string &host, int port);
    ~DatabaseConnection();

    void getPostgresVersion();

private:
    pqxx::connection *conn;
};

#endif
