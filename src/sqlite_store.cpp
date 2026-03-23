#include "antropy_farm_gateway/sqlite_store.hpp"
#include <sqlite3.h>
#include <filesystem>
#include <stdexcept>

namespace antropy::gateway {
namespace {
void exec_or_throw(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        const std::string msg = err ? err : "sqlite error";
        sqlite3_free(err);
        throw std::runtime_error(msg);
    }
}
}

SqliteStore::SqliteStore(const std::string& db_path) {
    const auto path = std::filesystem::path(db_path);
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        const std::string msg = db_ ? sqlite3_errmsg(db_) : "failed to open sqlite database";
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error(msg);
    }

    exec_or_throw(db_,
        "CREATE TABLE IF NOT EXISTS sensor_readings ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "sensor_id INTEGER NOT NULL,"
        "temperature_c REAL NOT NULL,"
        "humidity_pct REAL NOT NULL,"
        "timestamp_ms INTEGER NOT NULL"
        ");");

    const char* insert_sql =
        "INSERT INTO sensor_readings(sensor_id, temperature_c, humidity_pct, timestamp_ms) VALUES(?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_, insert_sql, -1, &insert_stmt_, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
}

SqliteStore::~SqliteStore() {
    if (insert_stmt_) sqlite3_finalize(insert_stmt_);
    if (db_) sqlite3_close(db_);
}

void SqliteStore::insert_reading(const SensorReading& reading) {
    if (!db_ || !insert_stmt_) return;
    sqlite3_reset(insert_stmt_);
    sqlite3_clear_bindings(insert_stmt_);
    const auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        reading.timestamp.time_since_epoch()).count();
    sqlite3_bind_int(insert_stmt_, 1, static_cast<int>(reading.sensor_id));
    sqlite3_bind_double(insert_stmt_, 2, reading.temperature_c);
    sqlite3_bind_double(insert_stmt_, 3, reading.humidity_pct);
    sqlite3_bind_int64(insert_stmt_, 4, static_cast<sqlite3_int64>(timestamp_ms));
    if (sqlite3_step(insert_stmt_) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
}

} // namespace antropy::gateway
