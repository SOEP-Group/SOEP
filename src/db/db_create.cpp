#include "db_create.h"
#include "postgreSQL/db_module.h"

std::unique_ptr<Database> CreateDB::createDatabase() {
    return std::make_unique<PostgreSQL>();
}