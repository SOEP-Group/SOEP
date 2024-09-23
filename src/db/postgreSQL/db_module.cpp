#include "db_module.h"
#include <iostream>

PostgreSQL::PostgreSQL() : connection(nullptr) {}

PostgreSQL::~PostgreSQL() {
    if (connection) {
        disconnect();
    }
}

bool PostgreSQL::connect(const std::string& connectionString) {
    try {
        connection = std::make_unique<pqxx::connection>(connectionString);
        if (connection->is_open()) {
            std::cout << "Connected to Biggest Benus elephent db" << std::endl;
            return true;
        } else {
            std::cerr << "Failed to open skibidi sigma connection" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Connection error: " << e.what() << std::endl;
        return false;
    }
}

void PostgreSQL::disconnect() {
    if (connection && connection->is_open()) {
        connection->disconnect();
        connection.reset();
        std::cout << "Disconnected from skibidi database." << std::endl;
    }
}

std::vector<std::string> PostgreSQL::executeQuery(const std::string& query) {
    std::vector<std::string> results;
    return results;
}
