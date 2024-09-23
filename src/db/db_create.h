#pragma once
#include "database.h"
#include <memory>

class CreateDB {
public:
    static std::unique_ptr<Database> createDatabase();
};