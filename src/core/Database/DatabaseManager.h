#pragma once

#include <string>
#include <memory>
#include <functional>

// Forward declare SQLite types
struct sqlite3;
struct sqlite3_stmt;

class DatabaseManager
{
public:
    DatabaseManager();
    ~DatabaseManager();

    // Database lifecycle
    bool Initialize(const std::string& databasePath);
    void Shutdown();
    bool IsConnected() const { return m_database != nullptr; }

    // Database operations
    bool ExecuteSQL(const std::string& sql);
    bool ExecuteSQL(const std::string& sql, const std::function<void(sqlite3_stmt*)>& bindCallback);
    
    // Query operations with result callback
    bool ExecuteQuery(const std::string& sql, const std::function<bool(sqlite3_stmt*)>& resultCallback);
    bool ExecuteQuery(const std::string& sql, 
                     const std::function<void(sqlite3_stmt*)>& bindCallback,
                     const std::function<bool(sqlite3_stmt*)>& resultCallback);

    // Transaction support
    bool BeginTransaction();
    bool CommitTransaction();
    bool RollbackTransaction();

    // Utility methods
    std::string GetLastError() const;
    int GetLastInsertRowID() const;
    int GetChangesCount() const;

    // Database info
    std::string GetDatabasePath() const { return m_databasePath; }
    bool TableExists(const std::string& tableName);
    
    // Schema versioning
    bool CreateVersionTable();
    int GetSchemaVersion();
    bool SetSchemaVersion(int version);

protected:
    // Internal helpers
    bool PrepareStatement(const std::string& sql, sqlite3_stmt** statement);
    void FinalizeStatement(sqlite3_stmt* statement);
    std::string GetSQLiteErrorMessage() const;

private:
    sqlite3* m_database;
    std::string m_databasePath;
    std::string m_lastError;
    bool m_inTransaction;

    // Non-copyable
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
};

// Helper class for RAII statement management
class SQLiteStatement
{
public:
    SQLiteStatement(sqlite3* db, const std::string& sql);
    ~SQLiteStatement();

    bool IsValid() const { return m_statement != nullptr; }
    sqlite3_stmt* Get() const { return m_statement; }
    operator sqlite3_stmt*() const { return m_statement; }

    // Binding methods
    bool BindInt(int index, int value);
    bool BindInt64(int index, int64_t value);
    bool BindDouble(int index, double value);
    bool BindText(int index, const std::string& value);
    bool BindBlob(int index, const void* data, int size);
    bool BindNull(int index);

    // Result methods
    int GetColumnInt(int index);
    int64_t GetColumnInt64(int index);
    double GetColumnDouble(int index);
    std::string GetColumnText(int index);
    const void* GetColumnBlob(int index, int& size);
    bool IsColumnNull(int index);

    // Execution
    bool Step();
    bool Reset();

private:
    sqlite3_stmt* m_statement;
    std::string m_sql;

    // Non-copyable
    SQLiteStatement(const SQLiteStatement&) = delete;
    SQLiteStatement& operator=(const SQLiteStatement&) = delete;
};