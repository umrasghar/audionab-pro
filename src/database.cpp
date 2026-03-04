#include "database.h"
#include "helpers.h"
#include "constants.h"
#include "../third_party/sqlite3/sqlite3.h"
#include <windows.h>

Database::~Database() {
    Close();
}

bool Database::Open() {
    if (db_) return true;

    InitializeCriticalSection(&cs_);
    csInit_ = true;

    std::wstring dbPath = Helpers::GetAppDataDir() + L"\\" + AppDef::DB_NAME;
    std::string dbPathUtf8 = Helpers::WideToUtf8(dbPath);

    int rc = sqlite3_open(dbPathUtf8.c_str(), &db_);
    if (rc != SQLITE_OK) {
        db_ = nullptr;
        return false;
    }

    // WAL mode for better concurrency
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    return CreateSchema();
}

void Database::Close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    if (csInit_) {
        DeleteCriticalSection(&cs_);
        csInit_ = false;
    }
}

bool Database::CreateSchema() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS history (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            input_path   TEXT NOT NULL,
            output_path  TEXT,
            output_format TEXT,
            bitrate      TEXT,
            input_size   INTEGER DEFAULT 0,
            output_size  INTEGER DEFAULT 0,
            duration     REAL DEFAULT 0,
            success      INTEGER DEFAULT 0,
            error        TEXT,
            timestamp    INTEGER DEFAULT 0,
            transcript   TEXT
        );
        CREATE INDEX IF NOT EXISTS idx_timestamp ON history(timestamp DESC);
    )";

    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK;
}

int64_t Database::AddEntry(const HistoryEntry& entry) {
    EnterCriticalSection(&cs_);

    const char* sql = R"(
        INSERT INTO history (input_path, output_path, output_format, bitrate,
                            input_size, output_size, duration, success, error, timestamp)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt = nullptr;
    int64_t rowId = -1;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        std::string inp  = Helpers::WideToUtf8(entry.inputPath);
        std::string outp = Helpers::WideToUtf8(entry.outputPath);
        std::string fmt  = Helpers::WideToUtf8(entry.outputFormat);
        std::string br   = Helpers::WideToUtf8(entry.bitrate);
        std::string err  = Helpers::WideToUtf8(entry.error);

        sqlite3_bind_text(stmt, 1, inp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, outp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, fmt.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, br.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 5, entry.inputSize);
        sqlite3_bind_int64(stmt, 6, entry.outputSize);
        sqlite3_bind_double(stmt, 7, entry.duration);
        sqlite3_bind_int(stmt, 8, entry.success ? 1 : 0);
        sqlite3_bind_text(stmt, 9, err.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 10, entry.timestamp);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            rowId = sqlite3_last_insert_rowid(db_);
        }
        sqlite3_finalize(stmt);
    }

    LeaveCriticalSection(&cs_);
    return rowId;
}

bool Database::UpdateEntry(const HistoryEntry& entry) {
    EnterCriticalSection(&cs_);

    const char* sql = R"(
        UPDATE history SET output_path=?, output_size=?, success=?, error=?, transcript=?
        WHERE id=?
    )";

    sqlite3_stmt* stmt = nullptr;
    bool ok = false;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        std::string outp = Helpers::WideToUtf8(entry.outputPath);
        std::string err  = Helpers::WideToUtf8(entry.error);
        std::string tr   = Helpers::WideToUtf8(entry.transcript);

        sqlite3_bind_text(stmt, 1, outp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, entry.outputSize);
        sqlite3_bind_int(stmt, 3, entry.success ? 1 : 0);
        sqlite3_bind_text(stmt, 4, err.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, tr.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 6, entry.id);

        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }

    LeaveCriticalSection(&cs_);
    return ok;
}

bool Database::DeleteEntry(int64_t id) {
    EnterCriticalSection(&cs_);
    char sql[128];
    snprintf(sql, sizeof(sql), "DELETE FROM history WHERE id=%lld", id);
    bool ok = sqlite3_exec(db_, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
    LeaveCriticalSection(&cs_);
    return ok;
}

bool Database::ClearHistory() {
    EnterCriticalSection(&cs_);
    bool ok = sqlite3_exec(db_, "DELETE FROM history", nullptr, nullptr, nullptr) == SQLITE_OK;
    LeaveCriticalSection(&cs_);
    return ok;
}

static HistoryEntry RowToEntry(sqlite3_stmt* stmt) {
    HistoryEntry e;
    e.id           = sqlite3_column_int64(stmt, 0);
    e.inputPath    = Helpers::Utf8ToWide(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
    e.outputPath   = Helpers::Utf8ToWide(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
    e.outputFormat = Helpers::Utf8ToWide(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
    e.bitrate      = Helpers::Utf8ToWide(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
    e.inputSize    = sqlite3_column_int64(stmt, 5);
    e.outputSize   = sqlite3_column_int64(stmt, 6);
    e.duration     = sqlite3_column_double(stmt, 7);
    e.success      = sqlite3_column_int(stmt, 8) != 0;
    auto errText   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
    if (errText) e.error = Helpers::Utf8ToWide(errText);
    e.timestamp    = sqlite3_column_int64(stmt, 10);
    auto trText    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
    if (trText) e.transcript = Helpers::Utf8ToWide(trText);
    return e;
}

std::vector<HistoryEntry> Database::GetAll(const std::wstring& search, int filter) {
    EnterCriticalSection(&cs_);
    std::vector<HistoryEntry> results;

    std::string sql = "SELECT id, input_path, output_path, output_format, bitrate, "
                      "input_size, output_size, duration, success, error, timestamp, transcript "
                      "FROM history";

    std::vector<std::string> conditions;
    if (filter == 1) conditions.push_back("success=1");
    if (filter == 2) conditions.push_back("success=0");

    std::string searchUtf8;
    if (!search.empty()) {
        searchUtf8 = Helpers::WideToUtf8(search);
        conditions.push_back("(input_path LIKE ? OR output_path LIKE ?)");
    }

    if (!conditions.empty()) {
        sql += " WHERE ";
        for (size_t i = 0; i < conditions.size(); i++) {
            if (i > 0) sql += " AND ";
            sql += conditions[i];
        }
    }
    sql += " ORDER BY timestamp DESC";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (!searchUtf8.empty()) {
            std::string like = "%" + searchUtf8 + "%";
            sqlite3_bind_text(stmt, 1, like.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, like.c_str(), -1, SQLITE_TRANSIENT);
        }
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            results.push_back(RowToEntry(stmt));
        }
        sqlite3_finalize(stmt);
    }

    LeaveCriticalSection(&cs_);
    return results;
}

HistoryEntry Database::GetById(int64_t id) {
    EnterCriticalSection(&cs_);
    HistoryEntry entry;

    const char* sql = "SELECT id, input_path, output_path, output_format, bitrate, "
                      "input_size, output_size, duration, success, error, timestamp, transcript "
                      "FROM history WHERE id=?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            entry = RowToEntry(stmt);
        }
        sqlite3_finalize(stmt);
    }

    LeaveCriticalSection(&cs_);
    return entry;
}

int Database::GetTotalCount() {
    EnterCriticalSection(&cs_);
    int count = 0;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM history", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    LeaveCriticalSection(&cs_);
    return count;
}

int Database::GetSuccessCount() {
    EnterCriticalSection(&cs_);
    int count = 0;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM history WHERE success=1", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    LeaveCriticalSection(&cs_);
    return count;
}

int Database::GetFailedCount() {
    EnterCriticalSection(&cs_);
    int count = 0;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM history WHERE success=0", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    LeaveCriticalSection(&cs_);
    return count;
}

int64_t Database::GetTotalSavedBytes() {
    EnterCriticalSection(&cs_);
    int64_t total = 0;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT COALESCE(SUM(input_size - output_size), 0) FROM history WHERE success=1 AND output_size > 0";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) total = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
    }
    LeaveCriticalSection(&cs_);
    return total;
}
