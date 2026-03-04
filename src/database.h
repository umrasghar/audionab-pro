#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

struct sqlite3;

struct HistoryEntry {
    int64_t id = 0;
    std::wstring inputPath;
    std::wstring outputPath;
    std::wstring outputFormat;
    std::wstring bitrate;
    int64_t inputSize  = 0;
    int64_t outputSize = 0;
    double duration    = 0.0;
    bool success       = false;
    std::wstring error;
    int64_t timestamp  = 0;
    std::wstring transcript;
};

class Database {
public:
    ~Database();

    bool Open();
    void Close();

    // CRUD
    int64_t AddEntry(const HistoryEntry& entry);
    bool UpdateEntry(const HistoryEntry& entry);
    bool DeleteEntry(int64_t id);
    bool ClearHistory();

    // Queries
    std::vector<HistoryEntry> GetAll(const std::wstring& search = L"",
                                     int filter = 0); // 0=All, 1=Success, 2=Failed
    HistoryEntry GetById(int64_t id);

    // Stats
    int GetTotalCount();
    int GetSuccessCount();
    int GetFailedCount();
    int64_t GetTotalSavedBytes();

private:
    bool CreateSchema();
    sqlite3* db_ = nullptr;
    CRITICAL_SECTION cs_ = {};
    bool csInit_ = false;
};
