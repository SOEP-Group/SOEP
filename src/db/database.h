#pragma once
#include <string>
#include <vector>

class Database {
public:
    virtual ~Database() = default;

    virtual bool connect(const std::string& connectionString) = 0;

    virtual void disconnect() = 0;

    virtual std::vector<std::string> executeQuery(const std::string& query) = 0;
}