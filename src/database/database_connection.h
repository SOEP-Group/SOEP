#pragma once

#include <pqxx/pqxx>
#include <string>

class DatabaseConnection
{
public:
	DatabaseConnection(const std::string& dbname, const std::string& user, const std::string& password, const std::string& host, int port);
	~DatabaseConnection();

	void getPostgresVersion();

private:
	pqxx::connection* conn;
};
