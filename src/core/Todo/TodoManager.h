#pragma once

#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <chrono>
#include <unordered_map>
#include "imgui.h"

// Forward declarations
class AppConfig;

namespace Todo {

enum class Priority {
    Low = 0,
    Medium = 1,
    High = 2,
    Urgent = 3
};

enum class Status {
    Pending = 0,
    InProgress = 1,
    Completed = 2,
    Cancelled = 3
};

struct Task {
    std::string id;
    std::string title;
    std::string description;
    Priority priority = Priority::Medium;
    Status status = Status::Pending;
    std::string dueDate; // Format: YYYY-MM-DD
    std::string dueTime; // Format: HH:MM
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point completedAt;
    bool isAllDay = true;
    std::string category;
    std::vector<std::string> tags;
    
    Task() {
        createdAt = std::chrono::system_clock::now();
        id = GenerateTaskId();
    }
    
    Task(const std::string& taskTitle) : Task() {
        title = taskTitle;
    }
    
    bool IsCompleted() const { return status == Status::Completed; }
    bool IsOverdue() const;
    bool IsDueToday() const;
    bool IsDueTomorrow() const;
    std::string GetFormattedDueDate() const;
    ImVec4 GetPriorityColor() const;
    ImVec4 GetStatusColor() const;
    
private:
    static std::string GenerateTaskId();
};

struct DayTasks {
    std::string date; // YYYY-MM-DD
    std::vector<std::shared_ptr<Task>> tasks;
    
    void AddTask(std::shared_ptr<Task> task);
    void RemoveTask(const std::string& taskId);
    void SortTasksByPriority();
    void SortTasksByTime();
    int GetCompletedCount() const;
    int GetPendingCount() const;
};

struct DragDropState {
    bool isDragging = false;
    std::shared_ptr<Task> draggedTask;
    std::string sourceDate;
    std::string targetDate;
    int targetIndex = -1;
};

} // namespace Todo

class TodoManager {
public:
    TodoManager();
    ~TodoManager();
    
    // Initialization
    bool Initialize(AppConfig* config);
    void Shutdown();
    
    // Task operations
    std::shared_ptr<Todo::Task> CreateTask(const std::string& title, const std::string& dueDate = "");
    bool UpdateTask(std::shared_ptr<Todo::Task> task);
    bool DeleteTask(const std::string& taskId);
    std::shared_ptr<Todo::Task> FindTask(const std::string& taskId);
    
    // Task management
    bool CompleteTask(const std::string& taskId);
    bool ToggleTaskStatus(const std::string& taskId);
    bool MoveTask(const std::string& taskId, const std::string& newDate);
    bool ReorderTask(const std::string& taskId, const std::string& date, int newIndex);
    
    // Data access
    Todo::DayTasks* GetTasksForDate(const std::string& date);
    const Todo::DayTasks* GetTasksForDate(const std::string& date) const;
    std::vector<std::shared_ptr<Todo::Task>> GetTasksInDateRange(const std::string& startDate, const std::string& endDate) const;
    std::vector<std::shared_ptr<Todo::Task>> GetOverdueTasks() const;
    std::vector<std::shared_ptr<Todo::Task>> GetTodayTasks() const;
    std::vector<std::shared_ptr<Todo::Task>> GetUpcomingTasks(int days = 7) const;
    
    // Calendar navigation
    void SetCurrentDate(const std::string& date);
    std::string GetCurrentDate() const { return m_currentDate; }
    std::string GetTodayDate() const;
    std::string GetPreviousDay(const std::string& date);
    std::string GetNextDay(const std::string& date);
    std::string GetWeekStart(const std::string& date);
    
    // View settings
    enum class ViewMode {
        Daily = 0,
        Weekly = 1,
        Monthly = 2
    };
    
    void SetViewMode(ViewMode mode) { m_viewMode = mode; }
    ViewMode GetViewMode() const { return m_viewMode; }
    
    // Drag and drop
    Todo::DragDropState& GetDragDropState() { return m_dragDropState; }
    void StartDrag(std::shared_ptr<Todo::Task> task, const std::string& sourceDate);
    void UpdateDrag(const std::string& targetDate, int targetIndex);
    void EndDrag();
    
    // Event callbacks
    using TaskCallback = std::function<void(std::shared_ptr<Todo::Task>)>;
    using DayCallback = std::function<void(const std::string&)>;
    
    void SetOnTaskUpdated(TaskCallback callback) { m_onTaskUpdated = callback; }
    void SetOnTaskCompleted(TaskCallback callback) { m_onTaskCompleted = callback; }
    void SetOnDayChanged(DayCallback callback) { m_onDayChanged = callback; }
    
    // Statistics
    int GetTotalTaskCount() const;
    int GetCompletedTaskCount() const;
    int GetPendingTaskCount() const;
    int GetOverdueTaskCount() const;
    float GetCompletionRate() const;
    
    // Persistence
    bool SaveToFile();
    bool LoadFromFile();
    
private:
    AppConfig* m_config = nullptr;
    std::string m_currentDate;
    ViewMode m_viewMode = ViewMode::Daily;
    std::unordered_map<std::string, std::unique_ptr<Todo::DayTasks>> m_dayTasks; // date -> tasks
    Todo::DragDropState m_dragDropState;
    
    // Event callbacks
    TaskCallback m_onTaskUpdated;
    TaskCallback m_onTaskCompleted;
    DayCallback m_onDayChanged;
    
    // Helper methods
    void EnsureDayExists(const std::string& date);
    std::string GetDataFilePath() const;
    void NotifyTaskUpdated(std::shared_ptr<Todo::Task> task);
    void NotifyTaskCompleted(std::shared_ptr<Todo::Task> task);
    void NotifyDayChanged(const std::string& date);
    
    // Date utilities
    bool IsValidDate(const std::string& date) const;
    std::tm ParseDate(const std::string& date) const;
    std::string FormatDate(const std::tm& date) const;
};