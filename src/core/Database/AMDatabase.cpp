#include "AMDatabase.h"
#include "core/Logger.h"
#include "sqlite3.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <random>

#include "DatabaseManager.h"

AMDatabase::AMDatabase(std::shared_ptr<DatabaseManager> dbManager)
    : m_dbManager(dbManager) {}

bool AMDatabase::Initialize() {
  if (!m_dbManager || !m_dbManager->IsConnected()) {
    Logger::Error("AMDatabase: Database manager not available");
    return false;
  }

  // Create tables if they don't exist
  if (!CreateTables()) {
    Logger::Error("KanbanDatabase: Failed to create tables");
    return false;
  }

  // Handle schema migrations
  int currentVersion = m_dbManager->GetSchemaVersion();
  if (currentVersion < CURRENT_SCHEMA_VERSION) {
    Logger::Info("KanbanDatabase: Migrating schema from version {} to {}",
                 currentVersion, CURRENT_SCHEMA_VERSION);

    if (!MigrateSchema(currentVersion, CURRENT_SCHEMA_VERSION)) {
      Logger::Error("KanbanDatabase: Schema migration failed");
      return false;
    }

    m_dbManager->SetSchemaVersion(CURRENT_SCHEMA_VERSION);
  }

  Logger::Info("KanbanDatabase initialized successfully");
  return true;
}

///////////////////////////////////////////////////////////////////
//  TABLE CREATION
///////////////////////////////////////////////////////////////////

bool AMDatabase::CreateTables()
{
    return CreateAMWindowTable() &&
           CreateAMWindowOpenedTable() &&
           CreateAMTaskExecutedTable() &&
           CreateAMSessionTable();
}

bool AMDatabase::CreateAMWindowTable()
{
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS am_window (
            id TEXT PRIMARY KEY,             -- UUID or string ID
            binary_name TEXT NOT NULL,
            display_name TEXT NOT NULL,
            category INTEGER NOT NULL,       -- maps to AMCategory enum

            is_deleted BOOLEAN NOT NULL DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            modified_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    return m_dbManager->ExecuteSQL(sql);
}

bool AMDatabase::CreateAMWindowOpenedTable()
{
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS am_window_opened (
            id TEXT PRIMARY KEY,
            window_id TEXT NOT NULL,         -- FK -> am_window.id
            start_time DATETIME NOT NULL,
            end_time DATETIME NOT NULL,
            duration_ms INTEGER NOT NULL,

            is_deleted BOOLEAN NOT NULL DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            modified_at DATETIME DEFAULT CURRENT_TIMESTAMP,

            FOREIGN KEY(window_id) REFERENCES am_window(id)
        );
    )";

    return m_dbManager->ExecuteSQL(sql);
}

bool AMDatabase::CreateAMTaskExecutedTable()
{
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS am_task_executed (
            id TEXT PRIMARY KEY,
            display_name TEXT NOT NULL,
            kanban_card_id TEXT,             -- optional FK to Kanban

            is_deleted BOOLEAN NOT NULL DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            modified_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    return m_dbManager->ExecuteSQL(sql);
}

bool AMDatabase::CreateAMSessionTable()
{
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS am_session (
            id TEXT PRIMARY KEY,
            pomodoro_session_id TEXT,        -- FK to pomodoro_session table
            category INTEGER NOT NULL,

            is_deleted BOOLEAN NOT NULL DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            modified_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    return m_dbManager->ExecuteSQL(sql);
}

bool AMDatabase::MigrateSchema(int fromVersion, int toVersion)
{
    // For now, we only have version 1, so no migrations needed
    // Future migrations would be handled here
    if (fromVersion >= toVersion)
    {
        return true; // No migration needed
    }

    bool success = true;
    
    for (int version = fromVersion; version < toVersion; ++version)
    {
        switch (version)
        {
            case 0: // Migrate from 0 to 1
                success = MigrateToVersion1();
                break;
            default:
                Logger::Warning("PomodoroDatabase: Unknown migration version {}", version + 1);
                break;
        }
        
        if (!success)
        {
            Logger::Error("PomodoroDatabase: Migration to version {} failed", version + 1);
            return false;
        }
    }

    return success;
}

bool AMDatabase::MigrateToVersion1()
{
    // This would contain migration logic for version 1
    // For now, tables are created fresh, so no migration needed
    return true;
}

///////////////////////////////////////////////////////////////////
//  DB Methods
///////////////////////////////////////////////////////////////////

// Utility method implementations
std::string AMDatabase::TimePointToString(const std::chrono::system_clock::time_point& tp)
{
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::chrono::system_clock::time_point AMDatabase::StringToTimePoint(const std::string& str)
{
    if (str.empty()) {
        return std::chrono::system_clock::now();
    }
    
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    
    if (ss.fail()) {
        return std::chrono::system_clock::now();
    }
    
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::string AMDatabase::GenerateUUID()
{
    // Simple UUID generation - you might want to use a proper UUID library
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);
    
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4";
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);
    
    return ss.str();
}

//////////////////////////////////////////////////////////////
// AMWindow CRUD Operations
//////////////////////////////////////////////////////////////

std::string AMDatabase::CreateAMWindow(const AM::AMWindow& window)
{
    std::string id = window.id.empty() ? GenerateUUID() : window.id;
    
    const std::string sql = R"(
        INSERT INTO am_window (id, binary_name, display_name, category, created_at, modified_at)
        VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)
    )";

    bool success = m_dbManager->ExecuteSQL(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, window.binaryName.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, window.displayName.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 4, static_cast<int>(window.category));
    });

    return success ? id : "";
}

std::unique_ptr<AM::AMWindow> AMDatabase::GetAMWindow(const std::string& id)
{
    const std::string sql = R"(
        SELECT id, binary_name, display_name, category 
        FROM am_window 
        WHERE id = ? AND is_deleted = 0
    )";

    std::unique_ptr<AM::AMWindow> window;

    m_dbManager->ExecuteQuery(sql, 
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        },
        [&](sqlite3_stmt* stmt) -> bool {
            window = std::make_unique<AM::AMWindow>();
            window->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            window->binaryName = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            window->displayName = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
            window->category = static_cast<AM::AMCategory>(sqlite3_column_int(stmt, 3));
            return false; // Only get first result
        });

    return window;
}

std::vector<std::unique_ptr<AM::AMWindow>> AMDatabase::GetAllAMWindows()
{
    std::vector<std::unique_ptr<AM::AMWindow>> windows;
    
    const std::string sql = R"(
        SELECT id, binary_name, display_name, category 
        FROM am_window 
        WHERE is_deleted = 0 
        ORDER BY created_at DESC
    )";

    m_dbManager->ExecuteQuery(sql, [&](sqlite3_stmt* stmt) -> bool {
        auto window = std::make_unique<AM::AMWindow>();
        window->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
        window->binaryName = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
        window->displayName = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
        window->category = static_cast<AM::AMCategory>(sqlite3_column_int(stmt, 3));
        
        windows.push_back(std::move(window));
        return true;
    });

    return windows;
}

std::vector<std::unique_ptr<AM::AMWindow>> AMDatabase::GetAMWindowsByCategory(AM::AMCategory category)
{
    std::vector<std::unique_ptr<AM::AMWindow>> windows;
    
    const std::string sql = R"(
        SELECT id, binary_name, display_name, category 
        FROM am_window 
        WHERE category = ? AND is_deleted = 0 
        ORDER BY created_at DESC
    )";

    m_dbManager->ExecuteQuery(sql,
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_int(stmt, 1, static_cast<int>(category));
        },
        [&](sqlite3_stmt* stmt) -> bool {
            auto window = std::make_unique<AM::AMWindow>();
            window->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            
            // Handle pomodoro_session_id - you'll need to load the PomodoroSession object
            // const char* pomodoroSessionId = (char*)sqlite3_column_text(stmt, 1);
            // if (pomodoroSessionId) {
            //     session->pomodoroSession = LoadPomodoroSession(pomodoroSessionId);
            // }
            
            window->category = static_cast<AM::AMCategory>(sqlite3_column_int(stmt, 2));
            
            windows.push_back(std::move(window));
            return true;
        });

    return windows;
}

std::vector<std::unique_ptr<AM::AMSession>> AMDatabase::GetAMSessionsByPomodoroSession(const std::string& pomodoroSessionId)
{
    std::vector<std::unique_ptr<AM::AMSession>> sessions;
    
    const std::string sql = R"(
        SELECT id, pomodoro_session_id, category 
        FROM am_session 
        WHERE pomodoro_session_id = ? AND is_deleted = 0 
        ORDER BY created_at DESC
    )";

    m_dbManager->ExecuteQuery(sql,
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, pomodoroSessionId.c_str(), -1, SQLITE_STATIC);
        },
        [&](sqlite3_stmt* stmt) -> bool {
            auto session = std::make_unique<AM::AMSession>();
            session->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            
            // Handle pomodoro_session_id - you'll need to load the PomodoroSession object
            // const char* pomodoroSessionId = (char*)sqlite3_column_text(stmt, 1);
            // if (pomodoroSessionId) {
            //     session->pomodoroSession = LoadPomodoroSession(pomodoroSessionId);
            // }
            
            session->category = static_cast<AM::AMCategory>(sqlite3_column_int(stmt, 2));
            
            sessions.push_back(std::move(session));
            return true;
        });

    return sessions;
}

bool AMDatabase::UpdateAMSession(const AM::AMSession& session)
{
    const std::string sql = R"(
        UPDATE am_session 
        SET pomodoro_session_id = ?, category = ?, modified_at = CURRENT_TIMESTAMP
        WHERE id = ? AND is_deleted = 0
    )";

    return m_dbManager->ExecuteSQL(sql, [&](sqlite3_stmt* stmt) {
        if (session.pomodoroSession) {
            // You'll need to determine how to get the ID from PomodoroSession
            // sqlite3_bind_text(stmt, 1, session.pomodoroSession->id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_null(stmt, 1); // For now, bind null - you'll need to adjust this
        } else {
            sqlite3_bind_null(stmt, 1);
        }
        
        sqlite3_bind_int(stmt, 2, static_cast<int>(session.category));
        sqlite3_bind_text(stmt, 3, session.id.c_str(), -1, SQLITE_STATIC);
    });
}

bool AMDatabase::DeleteAMSession(const std::string& id)
{
    const std::string sql = R"(
        UPDATE am_session 
        SET is_deleted = 1, modified_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";

    return m_dbManager->ExecuteSQL(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
    });
}

//////////////////////////////////////////////////////////////
// Additional Helper Methods for AMSession
//////////////////////////////////////////////////////////////

// You might want to implement these helper methods to fully populate AMSession objects:

/*
bool AMDatabase::LoadTasksForSession(AM::AMSession& session)
{
    // Implementation to load tasks associated with this session
    // This would require a junction table or additional foreign key relationships
    return true;
}

bool AMDatabase::LoadWindowsForSession(AM::AMSession& session)
{
    // Implementation to load windows associated with this session
    // This would require a junction table or additional foreign key relationships
    return true;
}

std::shared_ptr<PomodoroSession> AMDatabase::LoadPomodoroSession(const std::string& pomodoroSessionId)
{
    // Implementation to load PomodoroSession by ID
    // You'll need access to PomodoroDatabase for this
    return nullptr;
}
*/

//////////////////////////////////////////////////////////////
// Batch Operations
//////////////////////////////////////////////////////////////

bool AMDatabase::CreateMultipleAMWindows(const std::vector<AM::AMWindow>& windows)
{
    if (!m_dbManager->BeginTransaction()) {
        return false;
    }

    bool success = true;
    for (const auto& window : windows) {
        if (CreateAMWindow(window).empty()) {
            success = false;
            break;
        }
    }

    if (success) {
        return m_dbManager->CommitTransaction();
    } else {
        m_dbManager->RollbackTransaction();
        return false;
    }
}

bool AMDatabase::CreateMultipleAMWindowsOpened(const std::vector<AM::AMWindowOpened>& windowsOpened)
{
    if (!m_dbManager->BeginTransaction()) {
        return false;
    }

    bool success = true;
    for (const auto& windowOpened : windowsOpened) {
        if (CreateAMWindowOpened(windowOpened).empty()) {
            success = false;
            break;
        }
    }

    if (success) {
        return m_dbManager->CommitTransaction();
    } else {
        m_dbManager->RollbackTransaction();
        return false;
    }
}

bool AMDatabase::CreateMultipleAMTasksExecuted(const std::vector<AM::AMTaskExecuted>& tasks)
{
    if (!m_dbManager->BeginTransaction()) {
        return false;
    }

    bool success = true;
    for (const auto& task : tasks) {
        if (CreateAMTaskExecuted(task).empty()) {
            success = false;
            break;
        }
    }

    if (success) {
        return m_dbManager->CommitTransaction();
    } else {
        m_dbManager->RollbackTransaction();
        return false;
    }
}

//////////////////////////////////////////////////////////////
// Advanced Query Methods
//////////////////////////////////////////////////////////////

std::vector<std::unique_ptr<AM::AMWindowOpened>> AMDatabase::GetAMWindowsOpenedByDateRange(
    const std::chrono::system_clock::time_point& startDate,
    const std::chrono::system_clock::time_point& endDate)
{
    std::vector<std::unique_ptr<AM::AMWindowOpened>> windowsOpened;
    
    const std::string sql = R"(
        SELECT id, window_id, start_time, end_time, duration_ms 
        FROM am_window_opened 
        WHERE start_time >= ? AND end_time <= ? AND is_deleted = 0 
        ORDER BY start_time DESC
    )";

    m_dbManager->ExecuteQuery(sql,
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, TimePointToString(startDate).c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, TimePointToString(endDate).c_str(), -1, SQLITE_STATIC);
        },
        [&](sqlite3_stmt* stmt) -> bool {
            auto windowOpened = std::make_unique<AM::AMWindowOpened>();
            windowOpened->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            windowOpened->windowId = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            windowOpened->startTime = StringToTimePoint(sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "");
            windowOpened->endTime = StringToTimePoint(sqlite3_column_text(stmt, 3) ? (char*)sqlite3_column_text(stmt, 3) : "");
            windowOpened->durationMs = sqlite3_column_int64(stmt, 4);
            
            windowsOpened.push_back(std::move(windowOpened));
            return true;
        });

    return windowsOpened;
}

long long AMDatabase::GetTotalDurationByCategory(AM::AMCategory category)
{
    long long totalDuration = 0;
    
    const std::string sql = R"(
        SELECT SUM(awo.duration_ms) 
        FROM am_window_opened awo
        JOIN am_window aw ON awo.window_id = aw.id
        WHERE aw.category = ? AND awo.is_deleted = 0 AND aw.is_deleted = 0
    )";

    m_dbManager->ExecuteQuery(sql,
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_int(stmt, 1, static_cast<int>(category));
        },
        [&](sqlite3_stmt* stmt) -> bool {
            totalDuration = sqlite3_column_int64(stmt, 0);
            return false; // Only get first result
        });

    return totalDuration;
}

std::vector<std::pair<AM::AMCategory, long long>> AMDatabase::GetDurationByCategory()
{
    std::vector<std::pair<AM::AMCategory, long long>> categoryDurations;
    
    const std::string sql = R"(
        SELECT aw.category, SUM(awo.duration_ms) 
        FROM am_window_opened awo
        JOIN am_window aw ON awo.window_id = aw.id
        WHERE awo.is_deleted = 0 AND aw.is_deleted = 0
        GROUP BY aw.category
        ORDER BY SUM(awo.duration_ms) DESC
    )";

    m_dbManager->ExecuteQuery(sql, [&](sqlite3_stmt* stmt) -> bool {
        AM::AMCategory category = static_cast<AM::AMCategory>(sqlite3_column_int(stmt, 0));
        long long duration = sqlite3_column_int64(stmt, 1);
        
        categoryDurations.emplace_back(category, duration);
        return true;
    });

    return categoryDurations;
}

bool AMDatabase::UpdateAMWindow(const AM::AMWindow& window)
{
    const std::string sql = R"(
        UPDATE am_window 
        SET binary_name = ?, display_name = ?, category = ?, modified_at = CURRENT_TIMESTAMP
        WHERE id = ? AND is_deleted = 0
    )";

    return m_dbManager->ExecuteSQL(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, window.binaryName.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, window.displayName.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, static_cast<int>(window.category));
        sqlite3_bind_text(stmt, 4, window.id.c_str(), -1, SQLITE_STATIC);
    });
}

bool AMDatabase::DeleteAMWindow(const std::string& id)
{
    const std::string sql = R"(
        UPDATE am_window 
        SET is_deleted = 1, modified_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";

    return m_dbManager->ExecuteSQL(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
    });
}

//////////////////////////////////////////////////////////////
// AMWindowOpened CRUD Operations
//////////////////////////////////////////////////////////////

std::string AMDatabase::CreateAMWindowOpened(const AM::AMWindowOpened& windowOpened)
{
    std::string id = windowOpened.id.empty() ? GenerateUUID() : windowOpened.id;
    
    const std::string sql = R"(
        INSERT INTO am_window_opened (id, window_id, start_time, end_time, duration_ms, created_at, modified_at)
        VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)
    )";

    bool success = m_dbManager->ExecuteSQL(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, windowOpened.windowId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, TimePointToString(windowOpened.startTime).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, TimePointToString(windowOpened.endTime).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 5, windowOpened.durationMs);
    });

    return success ? id : "";
}

std::unique_ptr<AM::AMWindowOpened> AMDatabase::GetAMWindowOpened(const std::string& id)
{
    const std::string sql = R"(
        SELECT id, window_id, start_time, end_time, duration_ms 
        FROM am_window_opened 
        WHERE id = ? AND is_deleted = 0
    )";

    std::unique_ptr<AM::AMWindowOpened> windowOpened;

    m_dbManager->ExecuteQuery(sql, 
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        },
        [&](sqlite3_stmt* stmt) -> bool {
            windowOpened = std::make_unique<AM::AMWindowOpened>();
            windowOpened->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            windowOpened->windowId = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            windowOpened->startTime = StringToTimePoint(sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "");
            windowOpened->endTime = StringToTimePoint(sqlite3_column_text(stmt, 3) ? (char*)sqlite3_column_text(stmt, 3) : "");
            windowOpened->durationMs = sqlite3_column_int64(stmt, 4);
            return false; // Only get first result
        });

    return windowOpened;
}

std::vector<std::unique_ptr<AM::AMWindowOpened>> AMDatabase::GetAllAMWindowsOpened()
{
    std::vector<std::unique_ptr<AM::AMWindowOpened>> windowsOpened;
    
    const std::string sql = R"(
        SELECT id, window_id, start_time, end_time, duration_ms 
        FROM am_window_opened 
        WHERE is_deleted = 0 
        ORDER BY start_time DESC
    )";

    m_dbManager->ExecuteQuery(sql, [&](sqlite3_stmt* stmt) -> bool {
        auto windowOpened = std::make_unique<AM::AMWindowOpened>();
        windowOpened->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
        windowOpened->windowId = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
        windowOpened->startTime = StringToTimePoint(sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "");
        windowOpened->endTime = StringToTimePoint(sqlite3_column_text(stmt, 3) ? (char*)sqlite3_column_text(stmt, 3) : "");
        windowOpened->durationMs = sqlite3_column_int64(stmt, 4);
        
        windowsOpened.push_back(std::move(windowOpened));
        return true;
    });

    return windowsOpened;
}

std::vector<std::unique_ptr<AM::AMWindowOpened>> AMDatabase::GetAMWindowsOpenedByWindowId(const std::string& windowId)
{
    std::vector<std::unique_ptr<AM::AMWindowOpened>> windowsOpened;
    
    const std::string sql = R"(
        SELECT id, window_id, start_time, end_time, duration_ms 
        FROM am_window_opened 
        WHERE window_id = ? AND is_deleted = 0 
        ORDER BY start_time DESC
    )";

    m_dbManager->ExecuteQuery(sql,
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, windowId.c_str(), -1, SQLITE_STATIC);
        },
        [&](sqlite3_stmt* stmt) -> bool {
            auto windowOpened = std::make_unique<AM::AMWindowOpened>();
            windowOpened->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            windowOpened->windowId = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            windowOpened->startTime = StringToTimePoint(sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "");
            windowOpened->endTime = StringToTimePoint(sqlite3_column_text(stmt, 3) ? (char*)sqlite3_column_text(stmt, 3) : "");
            windowOpened->durationMs = sqlite3_column_int64(stmt, 4);
            
            windowsOpened.push_back(std::move(windowOpened));
            return true;
        });

    return windowsOpened;
}

bool AMDatabase::UpdateAMWindowOpened(const AM::AMWindowOpened& windowOpened)
{
    const std::string sql = R"(
        UPDATE am_window_opened 
        SET window_id = ?, start_time = ?, end_time = ?, duration_ms = ?, modified_at = CURRENT_TIMESTAMP
        WHERE id = ? AND is_deleted = 0
    )";

    return m_dbManager->ExecuteSQL(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, windowOpened.windowId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, TimePointToString(windowOpened.startTime).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, TimePointToString(windowOpened.endTime).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 4, windowOpened.durationMs);
        sqlite3_bind_text(stmt, 5, windowOpened.id.c_str(), -1, SQLITE_STATIC);
    });
}

bool AMDatabase::DeleteAMWindowOpened(const std::string& id)
{
    const std::string sql = R"(
        UPDATE am_window_opened 
        SET is_deleted = 1, modified_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";

    return m_dbManager->ExecuteSQL(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
    });
}

//////////////////////////////////////////////////////////////
// AMTaskExecuted CRUD Operations
//////////////////////////////////////////////////////////////

std::string AMDatabase::CreateAMTaskExecuted(const AM::AMTaskExecuted& task)
{
    std::string id = task.id.empty() ? GenerateUUID() : task.id;
    
    const std::string sql = R"(
        INSERT INTO am_task_executed (id, display_name, kanban_card_id, created_at, modified_at)
        VALUES (?, ?, ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)
    )";

    bool success = m_dbManager->ExecuteSQL(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, task.displayName.c_str(), -1, SQLITE_STATIC);
        if (task.kanbanCardId.has_value()) {
            sqlite3_bind_text(stmt, 3, task.kanbanCardId->c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 3);
        }
    });

    return success ? id : "";
}

std::unique_ptr<AM::AMTaskExecuted> AMDatabase::GetAMTaskExecuted(const std::string& id)
{
    const std::string sql = R"(
        SELECT id, display_name, kanban_card_id 
        FROM am_task_executed 
        WHERE id = ? AND is_deleted = 0
    )";

    std::unique_ptr<AM::AMTaskExecuted> task;

    m_dbManager->ExecuteQuery(sql, 
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        },
        [&](sqlite3_stmt* stmt) -> bool {
            task = std::make_unique<AM::AMTaskExecuted>();
            task->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            task->displayName = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            
            if (sqlite3_column_text(stmt, 2)) {
                task->kanbanCardId = std::string((char*)sqlite3_column_text(stmt, 2));
            } else {
                task->kanbanCardId = std::nullopt;
            }
            
            return false; // Only get first result
        });

    return task;
}

std::vector<std::unique_ptr<AM::AMTaskExecuted>> AMDatabase::GetAllAMTasksExecuted()
{
    std::vector<std::unique_ptr<AM::AMTaskExecuted>> tasks;
    
    const std::string sql = R"(
        SELECT id, display_name, kanban_card_id 
        FROM am_task_executed 
        WHERE is_deleted = 0 
        ORDER BY created_at DESC
    )";

    m_dbManager->ExecuteQuery(sql, [&](sqlite3_stmt* stmt) -> bool {
        auto task = std::make_unique<AM::AMTaskExecuted>();
        task->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
        task->displayName = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
        
        if (sqlite3_column_text(stmt, 2)) {
            task->kanbanCardId = std::string((char*)sqlite3_column_text(stmt, 2));
        } else {
            task->kanbanCardId = std::nullopt;
        }
        
        tasks.push_back(std::move(task));
        return true;
    });

    return tasks;
}

std::vector<std::unique_ptr<AM::AMTaskExecuted>> AMDatabase::GetAMTasksExecutedByKanbanCard(const std::string& kanbanCardId)
{
    std::vector<std::unique_ptr<AM::AMTaskExecuted>> tasks;
    
    const std::string sql = R"(
        SELECT id, display_name, kanban_card_id 
        FROM am_task_executed 
        WHERE kanban_card_id = ? AND is_deleted = 0 
        ORDER BY created_at DESC
    )";

    m_dbManager->ExecuteQuery(sql,
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, kanbanCardId.c_str(), -1, SQLITE_STATIC);
        },
        [&](sqlite3_stmt* stmt) -> bool {
            auto task = std::make_unique<AM::AMTaskExecuted>();
            task->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            task->displayName = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            task->kanbanCardId = std::string((char*)sqlite3_column_text(stmt, 2));
            
            tasks.push_back(std::move(task));
            return true;
        });

    return tasks;
}

bool AMDatabase::UpdateAMTaskExecuted(const AM::AMTaskExecuted& task)
{
    const std::string sql = R"(
        UPDATE am_task_executed 
        SET display_name = ?, kanban_card_id = ?, modified_at = CURRENT_TIMESTAMP
        WHERE id = ? AND is_deleted = 0
    )";

    return m_dbManager->ExecuteSQL(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, task.displayName.c_str(), -1, SQLITE_STATIC);
        if (task.kanbanCardId.has_value()) {
            sqlite3_bind_text(stmt, 2, task.kanbanCardId->c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 2);
        }
        sqlite3_bind_text(stmt, 3, task.id.c_str(), -1, SQLITE_STATIC);
    });
}

bool AMDatabase::DeleteAMTaskExecuted(const std::string& id)
{
    const std::string sql = R"(
        UPDATE am_task_executed 
        SET is_deleted = 1, modified_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";

    return m_dbManager->ExecuteSQL(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
    });
}

//////////////////////////////////////////////////////////////
// AMSession CRUD Operations
//////////////////////////////////////////////////////////////

std::string AMDatabase::CreateAMSession(const AM::AMSession& session)
{
    std::string id = session.id.empty() ? GenerateUUID() : session.id;
    
    const std::string sql = R"(
        INSERT INTO am_session (id, pomodoro_session_id, category, created_at, modified_at)
        VALUES (?, ?, ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)
    )";

    bool success = m_dbManager->ExecuteSQL(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        
        if (session.pomodoroSession) {
            // You'll need to determine how to get the ID from PomodoroSession
            // This assumes PomodoroSession has an id field
            // sqlite3_bind_text(stmt, 2, session.pomodoroSession->id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_null(stmt, 2); // For now, bind null - you'll need to adjust this
        } else {
            sqlite3_bind_null(stmt, 2);
        }
        
        sqlite3_bind_int(stmt, 3, static_cast<int>(session.category));
    });

    return success ? id : "";
}

std::unique_ptr<AM::AMSession> AMDatabase::GetAMSession(const std::string& id)
{
    const std::string sql = R"(
        SELECT id, pomodoro_session_id, category 
        FROM am_session 
        WHERE id = ? AND is_deleted = 0
    )";

    std::unique_ptr<AM::AMSession> session;

    m_dbManager->ExecuteQuery(sql, 
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        },
        [&](sqlite3_stmt* stmt) -> bool {
            session = std::make_unique<AM::AMSession>();
            session->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            
            // Handle pomodoro_session_id - you'll need to load the PomodoroSession object
            // const char* pomodoroSessionId = (char*)sqlite3_column_text(stmt, 1);
            // if (pomodoroSessionId) {
            //     session->pomodoroSession = LoadPomodoroSession(pomodoroSessionId);
            // }
            
            session->category = static_cast<AM::AMCategory>(sqlite3_column_int(stmt, 2));
            
            // Load related tasks and windows
            // You might want to implement LoadTasksForSession and LoadWindowsForSession
            
            return false; // Only get first result
        });

    return session;
}

std::vector<std::unique_ptr<AM::AMSession>> AMDatabase::GetAllAMSessions()
{
    std::vector<std::unique_ptr<AM::AMSession>> sessions;
    
    const std::string sql = R"(
        SELECT id, pomodoro_session_id, category 
        FROM am_session 
        WHERE is_deleted = 0 
        ORDER BY created_at DESC
    )";

    m_dbManager->ExecuteQuery(sql, [&](sqlite3_stmt* stmt) -> bool {
        auto session = std::make_unique<AM::AMSession>();
        session->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
        
        // Handle pomodoro_session_id - you'll need to load the PomodoroSession object
        // const char* pomodoroSessionId = (char*)sqlite3_column_text(stmt, 1);
        // if (pomodoroSessionId) {
        //     session->pomodoroSession = LoadPomodoroSession(pomodoroSessionId);
        // }
        
        session->category = static_cast<AM::AMCategory>(sqlite3_column_int(stmt, 2));
        
        sessions.push_back(std::move(session));
        return true;
    });

    return sessions;
}

std::vector<std::unique_ptr<AM::AMSession>> AMDatabase::GetAMSessionsByCategory(AM::AMCategory category)
{
    std::vector<std::unique_ptr<AM::AMSession>> sessions;
    
    const std::string sql = R"(
        SELECT id, pomodoro_session_id, category 
        FROM am_session 
        WHERE category = ? AND is_deleted = 0 
        ORDER BY created_at DESC
    )";

    m_dbManager->ExecuteQuery(sql,
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_int(stmt, 1, static_cast<int>(category));
        },
        [&](sqlite3_stmt* stmt) -> bool {
            auto session = std::make_unique<AM::AMSession>();
            session->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            
            // Handle pomodoro_session_id - you'll need to load the PomodoroSession object
            // const char* pomodoroSessionId = (char*)sqlite3_column_text(stmt, 1);
            // if (pomodoroSessionId) {
            //     session->pomodoroSession = LoadPomodoroSession(pomodoroSessionId);
            // }
            
            session->category = static_cast<AM::AMCategory>(sqlite3_column_int(stmt, 2));
            
            sessions.push_back(std::move(session));
            return true;
        });

    return sessions;
}