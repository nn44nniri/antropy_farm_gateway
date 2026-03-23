#pragma once
#include "antropy_farm_gateway/types.hpp"
#include <string>

struct sqlite3;
struct sqlite3_stmt;

namespace antropy::gateway {

class SqliteStore {
  public:
    explicit SqliteStore(const std::string& db_path);
    ~SqliteStore();

    SqliteStore(const SqliteStore&) = delete;
    SqliteStore& operator=(const SqliteStore&) = delete;

    void insert_reading(const SensorReading& reading);

  private:
    sqlite3* db_{nullptr};
    sqlite3_stmt* insert_stmt_{nullptr};
};

} // namespace antropy::gateway
