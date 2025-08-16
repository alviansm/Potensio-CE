#include "DatabaseManager.h"
#include "core/Logger.h"
#include "sqlite3.h"
#include <filesystem>

DatabaseManager::DatabaseManager()
    : m_database(nullptr)
    , m_inTransaction(false)
{
}

DatabaseManager::~DatabaseManager()
{
    Shutdown();
}

bool DatabaseManager::Initialize(const std::string& databasePath)
{
    if (m_database != nullptr)
    {
        Logger::Warning("Database already initialized");
        return true;
    }
    else
    {
        Logger::Warning("Database hasn't initialized");
    }

    m_databasePath = databasePath;

    // Ensure directory exists
    std::filesystem::path dbPath(databasePath);
    std::filesystem::path dbDir = dbPath.parent_path();
    
    if (!dbDir.empty() && !std::filesystem::exists(dbDir))
    {
        std::error_code ec;
        if (!std::filesystem::create_directories(dbDir, ec))
        {
            m_lastError = "Failed to create database directory: " + ec.message();
            Logger::Error("Database initialization failed: {}", m_lastError);
            return false;
        }
    }

    // Open database
    int result = sqlite3_open(databasePath.c_str(), &m_database);
    if (result != SQLITE_OK)
    {
        m_lastError = "Failed to open database: " + GetSQLiteErrorMessage();
        Logger::Error("Database initialization failed: {}", m_lastError);
        
        if (m_database)
        {
            sqlite3_close(m_database);
            m_database = nullptr;
        }
        return false;
    }

    // Enable foreign keys
    if (!ExecuteSQL("PRAGMA foreign_keys = ON;"))
    {
        Logger::Warning("Failed to enable foreign keys");
    }

    // Set journal mode to WAL for better concurrency
    if (!ExecuteSQL("PRAGMA journal_mode = WAL;"))
    {
        Logger::Warning("Failed to set WAL journal mode");
    }

    // Create version table if it doesn't exist
    CreateVersionTable();

    Logger::Info("Database initialized successfully: {}", databasePath);
    return true;
}

void DatabaseManager::Shutdown()
{
    if (m_database)
    {
        if (m_inTransaction)
        {
            RollbackTransaction();
        }

        int result = sqlite3_close(m_database);
        if (result != SQLITE_OK)
        {
            Logger::Warning("Error closing database: {}", GetSQLiteErrorMessage());
        }
        
        m_database = nullptr;
        Logger::Debug("Database connection closed");
    }
}

bool DatabaseManager::ExecuteSQL(const std::string& sql)
{
    if (!m_database)
    {
        m_lastError = "Database not initialized";
        return false;
    }

    char* errorMessage = nullptr;
    int result = sqlite3_exec(m_database, sql.c_str(), nullptr, nullptr, &errorMessage);
    
    if (result != SQLITE_OK)
    {
        m_lastError = errorMessage ? errorMessage : "Unknown SQL execution error";
        Logger::Error("SQL execution failed: {} - SQL: {}", m_lastError, sql);
        
        if (errorMessage)
        {
            sqlite3_free(errorMessage);
        }
        return false;
    }

    return true;
}

bool DatabaseManager::ExecuteSQL(const std::string& sql, const std::function<void(sqlite3_stmt*)>& bindCallback)
{
    if (!m_database)
    {
        m_lastError = "Database not initialized";
        return false;
    }

    SQLiteStatement stmt(m_database, sql);
    if (!stmt.IsValid())
    {
        m_lastError = "Failed to prepare statement: " + GetSQLiteErrorMessage();
        return false;
    }

    if (bindCallback)
    {
        bindCallback(stmt.Get());
    }

    if (!stmt.Step())
    {
        // Check if it's actually an error or just no more rows
        int result = sqlite3_errcode(m_database);
        if (result != SQLITE_DONE && result != SQLITE_ROW)
        {
            m_lastError = "Statement execution failed: " + GetSQLiteErrorMessage();
            return false;
        }
    }

    return true;
}

bool DatabaseManager::ExecuteQuery(const std::string& sql, const std::function<bool(sqlite3_stmt*)>& resultCallback)
{
    return ExecuteQuery(sql, nullptr, resultCallback);
}

bool DatabaseManager::ExecuteQuery(const std::string& sql, 
                                  const std::function<void(sqlite3_stmt*)>& bindCallback,
                                  const std::function<bool(sqlite3_stmt*)>& resultCallback)
{
    if (!m_database)
    {
        m_lastError = "Database not initialized";
        return false;
    }

    SQLiteStatement stmt(m_database, sql);
    if (!stmt.IsValid())
    {
        m_lastError = "Failed to prepare statement: " + GetSQLiteErrorMessage();
        return false;
    }

    if (bindCallback)
    {
        bindCallback(stmt.Get());
    }

    while (stmt.Step())
    {
        if (resultCallback && !resultCallback(stmt.Get()))
        {
            break; // Callback requested to stop
        }
    }

    return true;
}

bool DatabaseManager::BeginTransaction()
{
    if (m_inTransaction)
    {
        Logger::Warning("Transaction already in progress");
        return false;
    }

    if (ExecuteSQL("BEGIN TRANSACTION;"))
    {
        m_inTransaction = true;
        return true;
    }

    return false;
}

bool DatabaseManager::CommitTransaction()
{
    if (!m_inTransaction)
    {
        Logger::Warning("No transaction in progress");
        return false;
    }

    bool success = ExecuteSQL("COMMIT;");
    m_inTransaction = false;
    return success;
}

bool DatabaseManager::RollbackTransaction()
{
    if (!m_inTransaction)
    {
        Logger::Warning("No transaction in progress");
        return false;
    }

    bool success = ExecuteSQL("ROLLBACK;");
    m_inTransaction = false;
    return success;
}

std::string DatabaseManager::GetLastError() const
{
    return m_lastError;
}

int DatabaseManager::GetLastInsertRowID() const
{
    if (!m_database)
        return -1;
    
    return static_cast<int>(sqlite3_last_insert_rowid(m_database));
}

int DatabaseManager::GetChangesCount() const
{
    if (!m_database)
        return 0;
    
    return sqlite3_changes(m_database);
}

bool DatabaseManager::TableExists(const std::string& tableName)
{
    if (!m_database)
        return false;

    bool exists = false;
    ExecuteQuery(
        "SELECT name FROM sqlite_master WHERE type='table' AND name=?;",
        [&tableName](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, SQLITE_STATIC);
        },
        [&exists](sqlite3_stmt* stmt) {
            exists = true;
            return false; // Stop after first result
        }
    );

    return exists;
}

bool DatabaseManager::CreateVersionTable()
{
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            version INTEGER NOT NULL DEFAULT 1,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    if (!ExecuteSQL(sql))
    {
        return false;
    }

    // Insert initial version if table is empty
    ExecuteSQL("INSERT OR IGNORE INTO schema_version (id, version) VALUES (1, 1);");
    
    return true;
}

int DatabaseManager::GetSchemaVersion()
{
    int version = 1;
    ExecuteQuery(
        "SELECT version FROM schema_version WHERE id = 1;",
        [&version](sqlite3_stmt* stmt) {
            version = sqlite3_column_int(stmt, 0);
            return false; // Stop after first result
        }
    );
    return version;
}

bool DatabaseManager::SetSchemaVersion(int version)
{
    return ExecuteSQL(
        "UPDATE schema_version SET version = ?, updated_at = CURRENT_TIMESTAMP WHERE id = 1;",
        [version](sqlite3_stmt* stmt) {
            sqlite3_bind_int(stmt, 1, version);
        }
    );
}

std::string DatabaseManager::GetSQLiteErrorMessage() const
{
    if (!m_database)
        return "Database not initialized";
    
    const char* error = sqlite3_errmsg(m_database);
    return error ? error : "Unknown error";
}

// SQLiteStatement implementation
SQLiteStatement::SQLiteStatement(sqlite3* db, const std::string& sql)
    : m_statement(nullptr)
    , m_sql(sql)
{
    if (db)
    {
        int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &m_statement, nullptr);
        if (result != SQLITE_OK)
        {
            Logger::Error("Failed to prepare SQL statement: {} - SQL: {}", sqlite3_errmsg(db), sql);
        }
    }
}

SQLiteStatement::~SQLiteStatement()
{
    if (m_statement)
    {
        sqlite3_finalize(m_statement);
    }
}

bool SQLiteStatement::BindInt(int index, int value)
{
    if (!m_statement) return false;
    return sqlite3_bind_int(m_statement, index, value) == SQLITE_OK;
}

bool SQLiteStatement::BindInt64(int index, int64_t value)
{
    if (!m_statement) return false;
    return sqlite3_bind_int64(m_statement, index, value) == SQLITE_OK;
}

bool SQLiteStatement::BindDouble(int index, double value)
{
    if (!m_statement) return false;
    return sqlite3_bind_double(m_statement, index, value) == SQLITE_OK;
}

bool SQLiteStatement::BindText(int index, const std::string& value)
{
    if (!m_statement) return false;
    return sqlite3_bind_text(m_statement, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

bool SQLiteStatement::BindBlob(int index, const void* data, int size)
{
    if (!m_statement) return false;
    return sqlite3_bind_blob(m_statement, index, data, size, SQLITE_TRANSIENT) == SQLITE_OK;
}

bool SQLiteStatement::BindNull(int index)
{
    if (!m_statement) return false;
    return sqlite3_bind_null(m_statement, index) == SQLITE_OK;
}

int SQLiteStatement::GetColumnInt(int index)
{
    if (!m_statement) return 0;
    return sqlite3_column_int(m_statement, index);
}

int64_t SQLiteStatement::GetColumnInt64(int index)
{
    if (!m_statement) return 0;
    return sqlite3_column_int64(m_statement, index);
}

double SQLiteStatement::GetColumnDouble(int index)
{
    if (!m_statement) return 0.0;
    return sqlite3_column_double(m_statement, index);
}

std::string SQLiteStatement::GetColumnText(int index)
{
    if (!m_statement) return "";
    const char* text = reinterpret_cast<const char*>(sqlite3_column_text(m_statement, index));
    return text ? text : "";
}

const void* SQLiteStatement::GetColumnBlob(int index, int& size)
{
    if (!m_statement) 
    {
        size = 0;
        return nullptr;
    }
    size = sqlite3_column_bytes(m_statement, index);
    return sqlite3_column_blob(m_statement, index);
}

bool SQLiteStatement::IsColumnNull(int index)
{
    if (!m_statement) return true;
    return sqlite3_column_type(m_statement, index) == SQLITE_NULL;
}

bool SQLiteStatement::Step()
{
    if (!m_statement) return false;
    int result = sqlite3_step(m_statement);
    return result == SQLITE_ROW;
}

bool SQLiteStatement::Reset()
{
    if (!m_statement) return false;
    return sqlite3_reset(m_statement) == SQLITE_OK;
}