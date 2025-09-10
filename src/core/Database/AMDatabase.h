#pragma once

#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <sstream>
#include <iomanip>

// Forward Declaration
class DatabaseManager;
struct Card; // FK -> struct to KanbanManager::Card (KanbanManager.h)
struct PomodoroSession; // FK -> struct to PomodoroSession (PomodoroDatabase.h)

namespace AM 
{
    enum class AMCategory {
        Unidentified = 0,
        Productivity,
        Communication,
        Learning,
        Entertainment,
        SocialMedia,
        Utilities,
        Personal,
        Creative,
        NewsAndReading,
        Idle,

        Count // This is only for reference on how many category that exist in this enum. Always put this on the last.
    };

    inline const char* ToString(AMCategory cat) {
        switch (cat) {
            case AMCategory::Unidentified:    return "Unidentified";
            case AMCategory::Productivity:    return "Productivity";
            case AMCategory::Communication:   return "Communication";
            case AMCategory::Learning:        return "Learning";
            case AMCategory::Entertainment:   return "Entertainment";
            case AMCategory::SocialMedia:     return "Social Media";
            case AMCategory::Utilities:       return "Utilities";
            case AMCategory::Personal:        return "Personal";
            case AMCategory::Creative:        return "Creative";
            case AMCategory::NewsAndReading:  return "News & Reading";
            case AMCategory::Idle:            return "Idle";
            default:                          return "Unknown";
        }
    }

    struct AMWindow {
        std::string id;        
        std::string binaryName;  // e.g. "chrome.exe"
        std::string displayName; // e.g. "Google Chrome"
        AMCategory category;
    };

    struct AMWindowOpened {
        std::string id;
        std::string windowId; // FK -> AMWindow.id
        std::chrono::system_clock::time_point startTime;
        std::chrono::system_clock::time_point endTime;
        long long durationMs = 0;
    };

    struct AMTaskExecuted {
        std::string id;
        std::string displayName;
        std::optional<std::string> kanbanCardId; // FK -> KanbanManager::Card
    };

    struct AMSession {
        std::string id;
        std::shared_ptr<PomodoroSession> pomodoroSession; // FK
        AMCategory category;

        std::vector<AMTaskExecuted> tasks;
        std::vector<AMWindowOpened> windows;
    };
}

// Activity Monitoring Database
class AMDatabase
{
public:
    AMDatabase(std::shared_ptr<DatabaseManager> dbManager);
    ~AMDatabase() = default;

    // Database initialization
    bool Initialize();

    // Utility methods
    std::string GetLastError() const { return m_lastError; }

// DB Methods
public:
    // AMWindow CRUD operations
    std::string CreateAMWindow(const AM::AMWindow& window);
    bool CreateMultipleAMWindows(const std::vector<AM::AMWindow>& windows);
    std::unique_ptr<AM::AMWindow> GetAMWindow(const std::string& id);
    std::vector<std::unique_ptr<AM::AMWindow>> GetAllAMWindows();
    std::vector<std::unique_ptr<AM::AMWindow>> GetAMWindowsByCategory(AM::AMCategory category);
    bool UpdateAMWindow(const AM::AMWindow& window);
    bool DeleteAMWindow(const std::string& id);

    // AMWindowOpened CRUD operations
    std::string CreateAMWindowOpened(const AM::AMWindowOpened& windowOpened);
    bool CreateMultipleAMWindowsOpened(const std::vector<AM::AMWindowOpened>& windowsOpened);
    std::unique_ptr<AM::AMWindowOpened> GetAMWindowOpened(const std::string& id);
    std::vector<std::unique_ptr<AM::AMWindowOpened>> GetAllAMWindowsOpened();
    std::vector<std::unique_ptr<AM::AMWindowOpened>> GetAMWindowsOpenedByWindowId(const std::string& windowId);
    bool UpdateAMWindowOpened(const AM::AMWindowOpened& windowOpened);
    bool DeleteAMWindowOpened(const std::string& id);
    std::vector<std::unique_ptr<AM::AMWindowOpened>> GetAMWindowsOpenedByDateRange(const std::chrono::system_clock::time_point& startDate,
                                                                                                const std::chrono::system_clock::time_point& endDate);

    // AMTaskExecuted CRUD operations
    std::string CreateAMTaskExecuted(const AM::AMTaskExecuted& task);
    std::unique_ptr<AM::AMTaskExecuted> GetAMTaskExecuted(const std::string& id);
    bool CreateMultipleAMTasksExecuted(const std::vector<AM::AMTaskExecuted>& tasks);
    std::vector<std::unique_ptr<AM::AMTaskExecuted>> GetAllAMTasksExecuted();
    std::vector<std::unique_ptr<AM::AMTaskExecuted>> GetAMTasksExecutedByKanbanCard(const std::string& kanbanCardId);
    bool UpdateAMTaskExecuted(const AM::AMTaskExecuted& task);
    bool DeleteAMTaskExecuted(const std::string& id);

    // AMSession CRUD operations
    std::string CreateAMSession(const AM::AMSession& session);
    std::unique_ptr<AM::AMSession> GetAMSession(const std::string& id);
    std::vector<std::unique_ptr<AM::AMSession>> GetAllAMSessions();
    std::vector<std::unique_ptr<AM::AMSession>> GetAMSessionsByCategory(AM::AMCategory category);
    std::vector<std::unique_ptr<AM::AMSession>> GetAMSessionsByPomodoroSession(const std::string& pomodoroSessionId);
    bool UpdateAMSession(const AM::AMSession& session);
    bool DeleteAMSession(const std::string& id);

    // Utility methods
    std::vector<std::pair<AM::AMCategory, long long>> GetDurationByCategory();
    long long GetTotalDurationByCategory(AM::AMCategory category);
    std::string TimePointToString(const std::chrono::system_clock::time_point& tp);
    std::chrono::system_clock::time_point StringToTimePoint(const std::string& str);
    std::string GenerateUUID();

// Private methods
private:
    bool CreateTables(); // Containt tables creations

    // Tables created
    bool CreateAMWindowTable();
    bool CreateAMWindowOpenedTable();
    bool CreateAMTaskExecutedTable();
    bool CreateAMSessionTable();
    bool CreateAMSessionNormalizationTable();

    bool MigrateSchema(int fromVersion, int toVersion);
    
    bool MigrateToVersion1();

private:
    std::shared_ptr<DatabaseManager> m_dbManager;
    std::string m_lastError;

    // Schema versioning
    static constexpr int CURRENT_SCHEMA_VERSION = 1;
};