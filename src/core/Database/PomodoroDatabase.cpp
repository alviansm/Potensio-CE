#include "PomodoroDatabase.h"
#include "core/Logger.h"
#include "sqlite3.h"
#include <sstream>
#include <iomanip>
#include <ctime>

PomodoroDatabase::PomodoroDatabase(std::shared_ptr<DatabaseManager> dbManager)
    : m_dbManager(dbManager)
{
}

bool PomodoroDatabase::Initialize()
{
    if (!m_dbManager || !m_dbManager->IsConnected())
    {
        Logger::Error("PomodoroDatabase: Database manager not available");
        return false;
    }

    // Create tables if they don't exist
    if (!CreateTables())
    {
        Logger::Error("PomodoroDatabase: Failed to create tables");
        return false;
    }

    // Handle schema migrations
    int currentVersion = m_dbManager->GetSchemaVersion();
    if (currentVersion < CURRENT_SCHEMA_VERSION)
    {
        Logger::Info("PomodoroDatabase: Migrating schema from version {} to {}", currentVersion, CURRENT_SCHEMA_VERSION);
        
        if (!MigrateSchema(currentVersion, CURRENT_SCHEMA_VERSION))
        {
            Logger::Error("PomodoroDatabase: Schema migration failed");
            return false;
        }
        
        m_dbManager->SetSchemaVersion(CURRENT_SCHEMA_VERSION);
    }

    Logger::Info("PomodoroDatabase initialized successfully");
    return true;
}

bool PomodoroDatabase::CreateTables()
{
    return CreateConfigurationTable() && 
           CreateSessionsTable() && 
           CreateStatisticsTable();
}

bool PomodoroDatabase::CreateConfigurationTable()
{
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS pomodoro_configuration (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            work_duration_minutes INTEGER NOT NULL DEFAULT 25,
            short_break_minutes INTEGER NOT NULL DEFAULT 5,
            long_break_minutes INTEGER NOT NULL DEFAULT 15,
            total_sessions INTEGER NOT NULL DEFAULT 8,
            sessions_before_long_break INTEGER NOT NULL DEFAULT 4,
            auto_start_next_session BOOLEAN NOT NULL DEFAULT 1,
            
            -- Color settings (RGB values 0.0-1.0)
            color_high_r REAL NOT NULL DEFAULT 0.0,
            color_high_g REAL NOT NULL DEFAULT 1.0,
            color_high_b REAL NOT NULL DEFAULT 0.0,
            
            color_medium_r REAL NOT NULL DEFAULT 1.0,
            color_medium_g REAL NOT NULL DEFAULT 1.0,
            color_medium_b REAL NOT NULL DEFAULT 0.0,
            
            color_low_r REAL NOT NULL DEFAULT 1.0,
            color_low_g REAL NOT NULL DEFAULT 0.5,
            color_low_b REAL NOT NULL DEFAULT 0.0,
            
            color_critical_r REAL NOT NULL DEFAULT 1.0,
            color_critical_g REAL NOT NULL DEFAULT 0.0,
            color_critical_b REAL NOT NULL DEFAULT 0.0,
            
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    return m_dbManager->ExecuteSQL(sql);
}

bool PomodoroDatabase::CreateSessionsTable()
{
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS pomodoro_sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            session_type TEXT NOT NULL CHECK (session_type IN ('work', 'short_break', 'long_break')),
            session_number INTEGER NOT NULL,
            start_time DATETIME NOT NULL,
            end_time DATETIME,
            completed BOOLEAN NOT NULL DEFAULT 0,
            paused_seconds INTEGER NOT NULL DEFAULT 0,
            date TEXT NOT NULL, -- YYYY-MM-DD format
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
        
        CREATE INDEX IF NOT EXISTS idx_pomodoro_sessions_date ON pomodoro_sessions(date);
        CREATE INDEX IF NOT EXISTS idx_pomodoro_sessions_type ON pomodoro_sessions(session_type);
        CREATE INDEX IF NOT EXISTS idx_pomodoro_sessions_completed ON pomodoro_sessions(completed);
    )";

    return m_dbManager->ExecuteSQL(sql);
}

bool PomodoroDatabase::CreateStatisticsTable()
{
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS pomodoro_statistics (
            date TEXT PRIMARY KEY, -- YYYY-MM-DD format
            total_sessions INTEGER NOT NULL DEFAULT 0,
            completed_sessions INTEGER NOT NULL DEFAULT 0,
            total_work_time INTEGER NOT NULL DEFAULT 0, -- in minutes
            total_break_time INTEGER NOT NULL DEFAULT 0, -- in minutes
            total_paused_time INTEGER NOT NULL DEFAULT 0, -- in seconds
            last_updated DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    return m_dbManager->ExecuteSQL(sql);
}

bool PomodoroDatabase::MigrateSchema(int fromVersion, int toVersion)
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

bool PomodoroDatabase::MigrateToVersion1()
{
    // This would contain migration logic for version 1
    // For now, tables are created fresh, so no migration needed
    return true;
}

bool PomodoroDatabase::SaveConfiguration(const PomodoroTimer::PomodoroConfig& config)
{
    const std::string sql = R"(
        INSERT OR REPLACE INTO pomodoro_configuration (
            id, work_duration_minutes, short_break_minutes, long_break_minutes,
            total_sessions, sessions_before_long_break, auto_start_next_session,
            color_high_r, color_high_g, color_high_b,
            color_medium_r, color_medium_g, color_medium_b,
            color_low_r, color_low_g, color_low_b,
            color_critical_r, color_critical_g, color_critical_b,
            updated_at
        ) VALUES (
            1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP
        );
    )";

    Logger::Debug("TEST: ", config.workDurationMinutes);

    return m_dbManager->ExecuteSQL(sql, [&config](sqlite3_stmt* stmt) {
        sqlite3_bind_int(stmt, 1, config.workDurationMinutes);
        sqlite3_bind_int(stmt, 2, config.shortBreakMinutes);
        sqlite3_bind_int(stmt, 3, config.longBreakMinutes);
        sqlite3_bind_int(stmt, 4, config.totalSessions);
        sqlite3_bind_int(stmt, 5, config.sessionsBeforeLongBreak);
        sqlite3_bind_int(stmt, 6, config.autoStartNextSession ? 1 : 0);
        
        // Color high
        sqlite3_bind_double(stmt, 7, config.colorHigh.r);
        sqlite3_bind_double(stmt, 8, config.colorHigh.g);
        sqlite3_bind_double(stmt, 9, config.colorHigh.b);
        
        // Color medium
        sqlite3_bind_double(stmt, 10, config.colorMedium.r);
        sqlite3_bind_double(stmt, 11, config.colorMedium.g);
        sqlite3_bind_double(stmt, 12, config.colorMedium.b);
        
        // Color low
        sqlite3_bind_double(stmt, 13, config.colorLow.r);
        sqlite3_bind_double(stmt, 14, config.colorLow.g);
        sqlite3_bind_double(stmt, 15, config.colorLow.b);
        
        // Color critical
        sqlite3_bind_double(stmt, 16, config.colorCritical.r);
        sqlite3_bind_double(stmt, 17, config.colorCritical.g);
        sqlite3_bind_double(stmt, 18, config.colorCritical.b);
    });
}

bool PomodoroDatabase::LoadConfiguration(PomodoroTimer::PomodoroConfig& config)
{
    const std::string sql = R"(
        SELECT work_duration_minutes, short_break_minutes, long_break_minutes,
               total_sessions, sessions_before_long_break, auto_start_next_session,
               color_high_r, color_high_g, color_high_b,
               color_medium_r, color_medium_g, color_medium_b,
               color_low_r, color_low_g, color_low_b,
               color_critical_r, color_critical_g, color_critical_b
        FROM pomodoro_configuration WHERE id = 1;
    )";

    bool found = false;
    
    bool success = m_dbManager->ExecuteQuery(sql, [&config, &found](sqlite3_stmt* stmt) {
        config.workDurationMinutes = sqlite3_column_int(stmt, 0);
        config.shortBreakMinutes = sqlite3_column_int(stmt, 1);
        config.longBreakMinutes = sqlite3_column_int(stmt, 2);
        config.totalSessions = sqlite3_column_int(stmt, 3);
        config.sessionsBeforeLongBreak = sqlite3_column_int(stmt, 4);
        config.autoStartNextSession = sqlite3_column_int(stmt, 5) != 0;
        
        // Color high
        config.colorHigh.r = static_cast<float>(sqlite3_column_double(stmt, 6));
        config.colorHigh.g = static_cast<float>(sqlite3_column_double(stmt, 7));
        config.colorHigh.b = static_cast<float>(sqlite3_column_double(stmt, 8));
        
        // Color medium
        config.colorMedium.r = static_cast<float>(sqlite3_column_double(stmt, 9));
        config.colorMedium.g = static_cast<float>(sqlite3_column_double(stmt, 10));
        config.colorMedium.b = static_cast<float>(sqlite3_column_double(stmt, 11));
        
        // Color low
        config.colorLow.r = static_cast<float>(sqlite3_column_double(stmt, 12));
        config.colorLow.g = static_cast<float>(sqlite3_column_double(stmt, 13));
        config.colorLow.b = static_cast<float>(sqlite3_column_double(stmt, 14));
        
        // Color critical
        config.colorCritical.r = static_cast<float>(sqlite3_column_double(stmt, 15));
        config.colorCritical.g = static_cast<float>(sqlite3_column_double(stmt, 16));
        config.colorCritical.b = static_cast<float>(sqlite3_column_double(stmt, 17));
        
        found = true;
        return false; // Stop after first row
    });

    return success && found;
}

bool PomodoroDatabase::ConfigurationExists()
{
    const std::string sql = "SELECT COUNT(*) FROM pomodoro_configuration WHERE id = 1;";
    
    int count = 0;
    m_dbManager->ExecuteQuery(sql, [&count](sqlite3_stmt* stmt) {
        count = sqlite3_column_int(stmt, 0);
        return false;
    });
    
    return count > 0;
}

int PomodoroDatabase::StartSession(const std::string& sessionType, int sessionNumber, const std::string& date)
{
    const std::string sql = R"(
        INSERT INTO pomodoro_sessions (session_type, session_number, start_time, date)
        VALUES (?, ?, CURRENT_TIMESTAMP, ?);
    )";

    bool success = m_dbManager->ExecuteSQL(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, sessionType.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, sessionNumber);
        sqlite3_bind_text(stmt, 3, date.c_str(), -1, SQLITE_STATIC);
    });

    if (success)
    {
        int sessionId = m_dbManager->GetLastInsertRowID();
        Logger::Debug("PomodoroDatabase: Started session {} (ID: {}) for {}", sessionType, sessionId, date);
        return sessionId;
    }

    return -1;
}

bool PomodoroDatabase::EndSession(int sessionId, bool completed, int pausedSeconds)
{
    const std::string sql = R"(
        UPDATE pomodoro_sessions 
        SET end_time = CURRENT_TIMESTAMP, completed = ?, paused_seconds = ?
        WHERE id = ?;
    )";

    bool success = m_dbManager->ExecuteSQL(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_int(stmt, 1, completed ? 1 : 0);
        sqlite3_bind_int(stmt, 2, pausedSeconds);
        sqlite3_bind_int(stmt, 3, sessionId);
    });

    if (success)
    {
        Logger::Debug("PomodoroDatabase: Ended session {} - Completed: {}, Paused: {}s", 
                     sessionId, completed, pausedSeconds);
        
        // Update daily statistics for the session's date
        PomodoroSession session = GetSession(sessionId);
        if (!session.date.empty())
        {
            UpdateDailyStatistics(session.date);
        }
    }

    return success;
}

bool PomodoroDatabase::UpdateSessionPausedTime(int sessionId, int pausedSeconds)
{
    const std::string sql = "UPDATE pomodoro_sessions SET paused_seconds = ? WHERE id = ?;";

    return m_dbManager->ExecuteSQL(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_int(stmt, 1, pausedSeconds);
        sqlite3_bind_int(stmt, 2, sessionId);
    });
}

std::vector<PomodoroSession> PomodoroDatabase::GetSessionsForDate(const std::string& date)
{
    const std::string sql = R"(
        SELECT id, session_type, session_number, start_time, end_time, completed, paused_seconds, date
        FROM pomodoro_sessions 
        WHERE date = ?
        ORDER BY start_time ASC;
    )";

    std::vector<PomodoroSession> sessions;
    
    m_dbManager->ExecuteQuery(sql, 
        [&date](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, date.c_str(), -1, SQLITE_STATIC);
        },
        [&sessions, this](sqlite3_stmt* stmt) {
            PomodoroSession session;
            session.id = sqlite3_column_int(stmt, 0);
            session.sessionType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            session.sessionNumber = sqlite3_column_int(stmt, 2);
            
            std::string startTimeStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            session.startTime = ParseDateTime(startTimeStr);
            
            if (sqlite3_column_type(stmt, 4) != SQLITE_NULL)
            {
                std::string endTimeStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                session.endTime = ParseDateTime(endTimeStr);
            }
            
            session.completed = sqlite3_column_int(stmt, 5) != 0;
            session.pausedSeconds = sqlite3_column_int(stmt, 6);
            session.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            
            sessions.push_back(session);
            return true; // Continue
        }
    );

    return sessions;
}

std::vector<PomodoroSession> PomodoroDatabase::GetSessionsForDateRange(const std::string& startDate, const std::string& endDate)
{
    const std::string sql = R"(
        SELECT id, session_type, session_number, start_time, end_time, completed, paused_seconds, date
        FROM pomodoro_sessions 
        WHERE date BETWEEN ? AND ?
        ORDER BY date ASC, start_time ASC;
    )";

    std::vector<PomodoroSession> sessions;
    
    m_dbManager->ExecuteQuery(sql, 
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, startDate.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, endDate.c_str(), -1, SQLITE_STATIC);
        },
        [&sessions, this](sqlite3_stmt* stmt) {
            PomodoroSession session;
            session.id = sqlite3_column_int(stmt, 0);
            session.sessionType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            session.sessionNumber = sqlite3_column_int(stmt, 2);
            
            std::string startTimeStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            session.startTime = ParseDateTime(startTimeStr);
            
            if (sqlite3_column_type(stmt, 4) != SQLITE_NULL)
            {
                std::string endTimeStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                session.endTime = ParseDateTime(endTimeStr);
            }
            
            session.completed = sqlite3_column_int(stmt, 5) != 0;
            session.pausedSeconds = sqlite3_column_int(stmt, 6);
            session.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            
            sessions.push_back(session);
            return true; // Continue
        }
    );

    return sessions;
}

PomodoroSession PomodoroDatabase::GetSession(int sessionId)
{
    const std::string sql = R"(
        SELECT id, session_type, session_number, start_time, end_time, completed, paused_seconds, date
        FROM pomodoro_sessions 
        WHERE id = ?;
    )";

    PomodoroSession session;
    
    m_dbManager->ExecuteQuery(sql, 
        [sessionId](sqlite3_stmt* stmt) {
            sqlite3_bind_int(stmt, 1, sessionId);
        },
        [&session, this](sqlite3_stmt* stmt) {
            session.id = sqlite3_column_int(stmt, 0);
            session.sessionType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            session.sessionNumber = sqlite3_column_int(stmt, 2);
            
            std::string startTimeStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            session.startTime = ParseDateTime(startTimeStr);
            
            if (sqlite3_column_type(stmt, 4) != SQLITE_NULL)
            {
                std::string endTimeStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                session.endTime = ParseDateTime(endTimeStr);
            }
            
            session.completed = sqlite3_column_int(stmt, 5) != 0;
            session.pausedSeconds = sqlite3_column_int(stmt, 6);
            session.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            
            return false; // Stop after first row
        }
    );

    return session;
}

PomodoroSession PomodoroDatabase::GetLastSession()
{
    const std::string sql = R"(
        SELECT id, session_type, session_number, start_time, end_time, completed, paused_seconds, date
        FROM pomodoro_sessions 
        ORDER BY start_time DESC 
        LIMIT 1;
    )";

    PomodoroSession session;
    
    m_dbManager->ExecuteQuery(sql, [&session, this](sqlite3_stmt* stmt) {
        session.id = sqlite3_column_int(stmt, 0);
        session.sessionType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        session.sessionNumber = sqlite3_column_int(stmt, 2);
        
        std::string startTimeStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        session.startTime = ParseDateTime(startTimeStr);
        
        if (sqlite3_column_type(stmt, 4) != SQLITE_NULL)
        {
            std::string endTimeStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            session.endTime = ParseDateTime(endTimeStr);
        }
        
        session.completed = sqlite3_column_int(stmt, 5) != 0;
        session.pausedSeconds = sqlite3_column_int(stmt, 6);
        session.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        
        return false; // Stop after first row
    });

    return session;
}

bool PomodoroDatabase::UpdateDailyStatistics(const std::string& date)
{
    // Calculate statistics from sessions for this date
    const std::string sessionSQL = R"(
        SELECT 
            COUNT(*) as total_sessions,
            SUM(CASE WHEN completed = 1 THEN 1 ELSE 0 END) as completed_sessions,
            SUM(CASE WHEN session_type = 'work' AND completed = 1 THEN ? ELSE 0 END) as total_work_time,
            SUM(CASE WHEN session_type IN ('short_break', 'long_break') AND completed = 1 
                     THEN CASE WHEN session_type = 'short_break' THEN ? ELSE ? END 
                     ELSE 0 END) as total_break_time,
            SUM(paused_seconds) as total_paused_time
        FROM pomodoro_sessions 
        WHERE date = ?;
    )";

    // Load current configuration to get durations
    PomodoroTimer::PomodoroConfig config;
    if (!LoadConfiguration(config))
    {
        // Use defaults if configuration not found
        config = PomodoroTimer::PomodoroConfig();
    }

    int totalSessions = 0, completedSessions = 0, totalWorkTime = 0, totalBreakTime = 0, totalPausedTime = 0;

    bool success = m_dbManager->ExecuteQuery(sessionSQL,
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_int(stmt, 1, config.workDurationMinutes);
            sqlite3_bind_int(stmt, 2, config.shortBreakMinutes);
            sqlite3_bind_int(stmt, 3, config.longBreakMinutes);
            sqlite3_bind_text(stmt, 4, date.c_str(), -1, SQLITE_STATIC);
        },
        [&](sqlite3_stmt* stmt) {
            totalSessions = sqlite3_column_int(stmt, 0);
            completedSessions = sqlite3_column_int(stmt, 1);
            totalWorkTime = sqlite3_column_int(stmt, 2);
            totalBreakTime = sqlite3_column_int(stmt, 3);
            totalPausedTime = sqlite3_column_int(stmt, 4);
            return false; // Stop after first row
        }
    );

    if (!success) return false;

    // Update or insert statistics
    const std::string updateSQL = R"(
        INSERT OR REPLACE INTO pomodoro_statistics 
        (date, total_sessions, completed_sessions, total_work_time, total_break_time, total_paused_time, last_updated)
        VALUES (?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP);
    )";

    return m_dbManager->ExecuteSQL(updateSQL, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, date.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, totalSessions);
        sqlite3_bind_int(stmt, 3, completedSessions);
        sqlite3_bind_int(stmt, 4, totalWorkTime);
        sqlite3_bind_int(stmt, 5, totalBreakTime);
        sqlite3_bind_int(stmt, 6, totalPausedTime);
    });
}

PomodoroStatistics PomodoroDatabase::GetDailyStatistics(const std::string& date)
{
    const std::string sql = R"(
        SELECT date, total_sessions, completed_sessions, total_work_time, total_break_time, total_paused_time, last_updated
        FROM pomodoro_statistics 
        WHERE date = ?;
    )";

    PomodoroStatistics stats;
    stats.date = date;

    m_dbManager->ExecuteQuery(sql,
        [&date](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, date.c_str(), -1, SQLITE_STATIC);
        },
        [&stats, this](sqlite3_stmt* stmt) {
            stats.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            stats.totalSessions = sqlite3_column_int(stmt, 1);
            stats.completedSessions = sqlite3_column_int(stmt, 2);
            stats.totalWorkTime = sqlite3_column_int(stmt, 3);
            stats.totalBreakTime = sqlite3_column_int(stmt, 4);
            stats.totalPausedTime = sqlite3_column_int(stmt, 5);
            
            std::string lastUpdatedStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            stats.lastUpdated = ParseDateTime(lastUpdatedStr);
            
            return false; // Stop after first row
        }
    );

    return stats;
}

bool PomodoroDatabase::ClearOldSessions(int daysToKeep)
{
    const std::string sql = R"(
        DELETE FROM pomodoro_sessions 
        WHERE date < date('now', '-' || ? || ' days');
    )";

    bool success = m_dbManager->ExecuteSQL(sql, [daysToKeep](sqlite3_stmt* stmt) {
        sqlite3_bind_int(stmt, 1, daysToKeep);
    });

    if (success)
    {
        int deletedCount = m_dbManager->GetChangesCount();
        Logger::Info("PomodoroDatabase: Cleared {} old sessions (older than {} days)", deletedCount, daysToKeep);
    }

    return success;
}

bool PomodoroDatabase::ClearAllData()
{
    m_dbManager->BeginTransaction();

    bool success = true;
    success &= m_dbManager->ExecuteSQL("DELETE FROM pomodoro_sessions;");
    success &= m_dbManager->ExecuteSQL("DELETE FROM pomodoro_statistics;");
    success &= m_dbManager->ExecuteSQL("DELETE FROM pomodoro_configuration;");

    if (success)
    {
        m_dbManager->CommitTransaction();
        Logger::Info("PomodoroDatabase: All data cleared successfully");
    }
    else
    {
        m_dbManager->RollbackTransaction();
        Logger::Error("PomodoroDatabase: Failed to clear all data");
    }

    return success;
}

std::string PomodoroDatabase::GetTodayDate() const
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
    return oss.str();
}

std::string PomodoroDatabase::GetYesterdayDate() const
{
    auto now = std::chrono::system_clock::now();
    auto yesterday = now - std::chrono::hours(24);
    auto time_t = std::chrono::system_clock::to_time_t(yesterday);
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
    return oss.str();
}

std::string PomodoroDatabase::FormatDateTime(const std::chrono::system_clock::time_point& timePoint) const
{
    auto time_t = std::chrono::system_clock::to_time_t(timePoint);
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::chrono::system_clock::time_point PomodoroDatabase::ParseDateTime(const std::string& dateTimeStr) const
{
    std::tm tm = {};
    std::istringstream ss(dateTimeStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    
    if (ss.fail()) 
    {
        // Return epoch if parsing fails
        return std::chrono::system_clock::time_point{};
    }
    
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

// Additional methods can be implemented as needed
bool PomodoroDatabase::ExportData(const std::string& filePath)
{
    // TODO: Implement data export functionality
    Logger::Info("PomodoroDatabase: Export data to {} (not yet implemented)", filePath);
    return false;
}

bool PomodoroDatabase::ImportData(const std::string& filePath)
{
    // TODO: Implement data import functionality
    Logger::Info("PomodoroDatabase: Import data from {} (not yet implemented)", filePath);
    return false;
}