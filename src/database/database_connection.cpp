#include "pch.h"
#include "database_connection.h"

DatabaseConnection::DatabaseConnection(const std::string& dbname, const std::string& user, const std::string& password, const std::string& host, int port)
{
	try
	{
		conn = new pqxx::connection("dbname=" + dbname + " user=" + user + " password=" + password + " host=" + host + " port=" + std::to_string(port));

		if (conn->is_open())
		{
			std::cout << "Connected to the database: " << conn->dbname() << std::endl;
		}
		else
		{
			std::cerr << "Failed to connect to the database." << std::endl;
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "Connection error: " << e.what() << std::endl;
		conn = nullptr;
	}
}

DatabaseConnection::~DatabaseConnection()
{
	if (conn)
	{
		delete conn; // This will automatically close the connection
	}
}

void DatabaseConnection::getPostgresVersion()
{
	if (conn && conn->is_open())
	{
		pqxx::nontransaction txn(*conn);
		pqxx::result res = txn.exec("SELECT version()");
		for (const auto& row : res)
		{
			std::cout << "PostgreSQL version: " << row[0].c_str() << std::endl;
		}
	}
}
