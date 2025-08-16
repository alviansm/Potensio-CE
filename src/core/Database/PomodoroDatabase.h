#pragma once

#include "DatabaseManager.h"
#include "core/Timer/PomodoroTimer.h"
#include <memory>
#include <vector>
#include <chrono>

struct PomodoroSession
{
    int id = -1;
    std::string sessionType; // "work", "short_break", "long_break"
    int sessionNumber = 1;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    bool completed = false;
    int pausedSeconds = 0;
    std::string date; // YYYY-MM-DD format
};

struct PomodoroStatistics
{
    std::string date; // YYYY-MM-DD format
    int totalSessions = 0;
    int completedSessions = 0;
    int totalWorkTime = 0; // in minutes
    int totalBreakTime = 0; // in minutes
    int totalPausedTime = 0; // in seconds
    std::chrono::system_clock::time_point lastUpdated;
};

class PomodoroDatabase
{
public:
    PomodoroDatabase(std::shared_ptr<DatabaseManager> dbManager);
    ~PomodoroDatabase() = default;

    // Database initialization
    bool Initialize();
    bool CreateTables();
    bool MigrateSchema(int fromVersion, int toVersion);

    // Configuration management
    bool SaveConfiguration(const PomodoroTimer::PomodoroConfig& config);
    bool LoadConfiguration(PomodoroTimer::PomodoroConfig& config);
    bool ConfigurationExists();

    // Session tracking
    int StartSession(const std::string& sessionType, int sessionNumber, const std::string& date);
    bool EndSession(int sessionId, bool completed, int pausedSeconds = 0);
    bool UpdateSessionPausedTime(int sessionId, int pausedSeconds);
    
    // Session queries
    std::vector<PomodoroSession> GetSessionsForDate(const std::string& date);
    std::vector<PomodoroSession> GetSessionsForDateRange(const std::string& startDate, const std::string& endDate);
    PomodoroSession GetSession(int sessionId);
    PomodoroSession GetLastSession();
    
    // Statistics
    bool UpdateDailyStatistics(const std::string& date);
    PomodoroStatistics GetDailyStatistics(const std::string& date);
    std::vector<PomodoroStatistics> GetStatisticsForDateRange(const std::string& startDate, const std::string& endDate);
    
    // Weekly/Monthly aggregations
    struct WeeklyStats
    {
        std::string weekStartDate; // Monday of the week
        int totalSessions = 0;
        int completedSessions = 0;
        int totalWorkMinutes = 0;
        float completionRate = 0.0f;
    };
    
    struct MonthlyStats
    {
        int year = 0;
        int month = 0;
        int totalSessions = 0;
        int completedSessions = 0;
        int totalWorkMinutes = 0;
        int activeDays = 0;
        float completionRate = 0.0f;
    };
    
    WeeklyStats GetWeeklyStatistics(const std::string& date); // Gets week containing this date
    MonthlyStats GetMonthlyStatistics(int year, int month);
    
    // Data management
    bool ClearOldSessions(int daysToKeep = 90); // Default keep 90 days
    bool ClearAllData();
    bool ExportData(const std::string& filePath);
    bool ImportData(const std::string& filePath);

    // Utility
    std::string GetTodayDate() const;
    std::string GetYesterdayDate() const;
    
private:
    std::shared_ptr<DatabaseManager> m_dbManager;
    
    // Schema versions
    static constexpr int CURRENT_SCHEMA_VERSION = 1;
    
    // Helper methods
    std::string FormatDateTime(const std::chrono::system_clock::time_point& timePoint) const;
    std::chrono::system_clock::time_point ParseDateTime(const std::string& dateTimeStr) const;
    std::string GetWeekStartDate(const std::string& date) const;
    
    // Table creation methods
    bool CreateConfigurationTable();
    bool CreateSessionsTable();
    bool CreateStatisticsTable();
    
    // Migration methods
    bool MigrateToVersion1();
};