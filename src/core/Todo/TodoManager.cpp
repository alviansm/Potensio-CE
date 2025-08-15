#include "core/Todo/TodoManager.h"
#include "app/AppConfig.h"
#include "core/Logger.h"
#include "core/Utils.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <random>

namespace Todo {

// Task Implementation
std::string Task::GenerateTaskId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    return "task_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

bool Task::IsOverdue() const {
    if (status == Status::Completed || dueDate.empty()) {
        return false;
    }
    
    auto now = std::chrono::system_clock::now();
    auto today = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_today = std::localtime(&today);
    
    char todayStr[32];
    std::strftime(todayStr, sizeof(todayStr), "%Y-%m-%d", tm_today);
    
    return dueDate < std::string(todayStr);
}

bool Task::IsDueToday() const {
    if (dueDate.empty()) return false;
    
    auto now = std::chrono::system_clock::now();
    auto today = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_today = std::localtime(&today);
    
    char todayStr[32];
    std::strftime(todayStr, sizeof(todayStr), "%Y-%m-%d", tm_today);
    
    return dueDate == std::string(todayStr);
}

bool Task::IsDueTomorrow() const {
    if (dueDate.empty()) return false;
    
    auto now = std::chrono::system_clock::now();
    auto tomorrow = now + std::chrono::hours(24);
    auto tomorrowTime = std::chrono::system_clock::to_time_t(tomorrow);
    std::tm* tm_tomorrow = std::localtime(&tomorrowTime);
    
    char tomorrowStr[32];
    std::strftime(tomorrowStr, sizeof(tomorrowStr), "%Y-%m-%d", tm_tomorrow);
    
    return dueDate == std::string(tomorrowStr);
}

std::string Task::GetFormattedDueDate() const {
    if (dueDate.empty()) return "No due date";
    
    if (IsDueToday()) return "Today";
    if (IsDueTomorrow()) return "Tomorrow";
    if (IsOverdue()) return "Overdue";
    
    // Parse the date and format it nicely
    std::istringstream ss(dueDate);
    std::tm tm = {};
    ss >> std::get_time(&tm, "%Y-%m-%d");
    
    if (ss.fail()) return dueDate; // Fallback to original string
    
    char formatted[32];
    std::strftime(formatted, sizeof(formatted), "%b %d", &tm);
    return std::string(formatted);
}

ImVec4 Task::GetPriorityColor() const {
    switch (priority) {
        case Priority::Low:    return ImVec4(0.5f, 0.8f, 0.5f, 1.0f); // Green (x=r, y=g, z=b, w=a)
        case Priority::Medium: return ImVec4(0.8f, 0.8f, 0.5f, 1.0f); // Yellow
        case Priority::High:   return ImVec4(0.9f, 0.6f, 0.3f, 1.0f); // Orange
        case Priority::Urgent: return ImVec4(0.9f, 0.3f, 0.3f, 1.0f); // Red
        default:               return ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray
    }
}

ImVec4 Task::GetStatusColor() const {
    switch (status) {
        case Status::Pending:    return ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray
        case Status::InProgress: return ImVec4(0.3f, 0.6f, 0.9f, 1.0f); // Blue
        case Status::Completed:  return ImVec4(0.4f, 0.8f, 0.4f, 1.0f); // Green
        case Status::Cancelled:  return ImVec4(0.8f, 0.4f, 0.4f, 1.0f); // Red
        default:                 return ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray
    }
}

// DayTasks Implementation
void DayTasks::AddTask(std::shared_ptr<Task> task) {
    if (task) {
        tasks.push_back(task);
        SortTasksByPriority();
    }
}

void DayTasks::RemoveTask(const std::string& taskId) {
    tasks.erase(std::remove_if(tasks.begin(), tasks.end(),
        [&taskId](const std::shared_ptr<Task>& task) {
            return task && task->id == taskId;
        }), tasks.end());
}

void DayTasks::SortTasksByPriority() {
    std::sort(tasks.begin(), tasks.end(),
        [](const std::shared_ptr<Task>& a, const std::shared_ptr<Task>& b) {
            if (!a || !b) return false;
            
            // Completed tasks go to bottom
            if (a->IsCompleted() != b->IsCompleted()) {
                return !a->IsCompleted();
            }
            
            // Sort by priority (higher priority first)
            if (a->priority != b->priority) {
                return static_cast<int>(a->priority) > static_cast<int>(b->priority);
            }
            
            // Sort by time if available
            if (!a->dueTime.empty() && !b->dueTime.empty()) {
                return a->dueTime < b->dueTime;
            }
            
            // Finally by creation time
            return a->createdAt < b->createdAt;
        });
}

void DayTasks::SortTasksByTime() {
    std::sort(tasks.begin(), tasks.end(),
        [](const std::shared_ptr<Task>& a, const std::shared_ptr<Task>& b) {
            if (!a || !b) return false;
            
            // Completed tasks go to bottom
            if (a->IsCompleted() != b->IsCompleted()) {
                return !a->IsCompleted();
            }
            
            // All-day tasks first, then timed tasks
            if (a->isAllDay != b->isAllDay) {
                return a->isAllDay;
            }
            
            // Sort by time
            if (!a->dueTime.empty() && !b->dueTime.empty()) {
                return a->dueTime < b->dueTime;
            }
            
            return a->createdAt < b->createdAt;
        });
}

int DayTasks::GetCompletedCount() const {
    return static_cast<int>(std::count_if(tasks.begin(), tasks.end(),
        [](const std::shared_ptr<Task>& task) {
            return task && task->IsCompleted();
        }));
}

int DayTasks::GetPendingCount() const {
    return static_cast<int>(std::count_if(tasks.begin(), tasks.end(),
        [](const std::shared_ptr<Task>& task) {
            return task && !task->IsCompleted();
        }));
}

} // namespace Todo

// TodoManager Implementation
TodoManager::TodoManager() {
    m_currentDate = GetTodayDate();
}

TodoManager::~TodoManager() {
    Shutdown();
}

bool TodoManager::Initialize(AppConfig* config) {
    m_config = config;
    
    // Load existing tasks
    LoadFromFile();
    
    Logger::Info("TodoManager initialized successfully");
    return true;
}

void TodoManager::Shutdown() {
    // Save tasks before shutdown
    SaveToFile();
    
    // Clear data
    m_dayTasks.clear();
    m_dragDropState = Todo::DragDropState();
    
    Logger::Debug("TodoManager shutdown complete");
}

std::shared_ptr<Todo::Task> TodoManager::CreateTask(const std::string& title, const std::string& dueDate) {
    auto task = std::make_shared<Todo::Task>(title);
    
    if (!dueDate.empty() && IsValidDate(dueDate)) {
        task->dueDate = dueDate;
    } else {
        task->dueDate = m_currentDate; // Default to current date
    }
    
    // Add to appropriate day
    EnsureDayExists(task->dueDate);
    m_dayTasks[task->dueDate]->AddTask(task);
    
    NotifyTaskUpdated(task);
    SaveToFile();
    
    Logger::Debug("Created task: {}", title);
    return task;
}

bool TodoManager::UpdateTask(std::shared_ptr<Todo::Task> task) {
    if (!task) return false;
    
    NotifyTaskUpdated(task);
    SaveToFile();
    
    Logger::Debug("Updated task: {}", task->title);
    return true;
}

bool TodoManager::DeleteTask(const std::string& taskId) {
    for (auto& [date, dayTasks] : m_dayTasks) {
        if (dayTasks) {
            auto& tasks = dayTasks->tasks;
            auto it = std::find_if(tasks.begin(), tasks.end(),
                [&taskId](const std::shared_ptr<Todo::Task>& task) {
                    return task && task->id == taskId;
                });
            
            if (it != tasks.end()) {
                tasks.erase(it);
                SaveToFile();
                Logger::Debug("Deleted task: {}", taskId);
                return true;
            }
        }
    }
    
    return false;
}

std::shared_ptr<Todo::Task> TodoManager::FindTask(const std::string& taskId) {
    for (auto& [date, dayTasks] : m_dayTasks) {
        if (dayTasks) {
            for (auto& task : dayTasks->tasks) {
                if (task && task->id == taskId) {
                    return task;
                }
            }
        }
    }
    return nullptr;
}

bool TodoManager::CompleteTask(const std::string& taskId) {
    auto task = FindTask(taskId);
    if (!task) return false;
    
    task->status = Todo::Status::Completed;
    task->completedAt = std::chrono::system_clock::now();
    
    // Resort the day's tasks
    for (auto& [date, dayTasks] : m_dayTasks) {
        if (dayTasks) {
            for (auto& dayTask : dayTasks->tasks) {
                if (dayTask && dayTask->id == taskId) {
                    dayTasks->SortTasksByPriority();
                    break;
                }
            }
        }
    }
    
    NotifyTaskCompleted(task);
    SaveToFile();
    
    Logger::Debug("Completed task: {}", task->title);
    return true;
}

bool TodoManager::ToggleTaskStatus(const std::string& taskId) {
    auto task = FindTask(taskId);
    if (!task) return false;
    
    if (task->status == Todo::Status::Completed) {
        task->status = Todo::Status::Pending;
    } else {
        task->status = Todo::Status::Completed;
        task->completedAt = std::chrono::system_clock::now();
        NotifyTaskCompleted(task);
    }
    
    // Resort the day's tasks
    for (auto& [date, dayTasks] : m_dayTasks) {
        if (dayTasks) {
            for (auto& dayTask : dayTasks->tasks) {
                if (dayTask && dayTask->id == taskId) {
                    dayTasks->SortTasksByPriority();
                    break;
                }
            }
        }
    }
    
    NotifyTaskUpdated(task);
    SaveToFile();
    
    Logger::Debug("Toggled task status: {}", task->title);
    return true;
}

bool TodoManager::MoveTask(const std::string& taskId, const std::string& newDate) {
    if (!IsValidDate(newDate)) return false;
    
    // Find and remove task from current location
    std::shared_ptr<Todo::Task> task;
    for (auto& [date, dayTasks] : m_dayTasks) {
        if (dayTasks) {
            auto& tasks = dayTasks->tasks;
            auto it = std::find_if(tasks.begin(), tasks.end(),
                [&taskId](const std::shared_ptr<Todo::Task>& t) {
                    return t && t->id == taskId;
                });
            
            if (it != tasks.end()) {
                task = *it;
                tasks.erase(it);
                break;
            }
        }
    }
    
    if (!task) return false;
    
    // Update task due date and add to new location
    task->dueDate = newDate;
    EnsureDayExists(newDate);
    m_dayTasks[newDate]->AddTask(task);
    
    NotifyTaskUpdated(task);
    SaveToFile();
    
    Logger::Debug("Moved task {} to {}", task->title, newDate);
    return true;
}

bool TodoManager::ReorderTask(const std::string& taskId, const std::string& date, int newIndex) {
    auto dayTasks = GetTasksForDate(date);
    if (!dayTasks) return false;
    
    auto& tasks = dayTasks->tasks;
    auto it = std::find_if(tasks.begin(), tasks.end(),
        [&taskId](const std::shared_ptr<Todo::Task>& task) {
            return task && task->id == taskId;
        });
    
    if (it == tasks.end()) return false;
    
    auto task = *it;
    tasks.erase(it);
    
    // Insert at new position
    if (newIndex >= 0 && newIndex <= static_cast<int>(tasks.size())) {
        tasks.insert(tasks.begin() + newIndex, task);
    } else {
        tasks.push_back(task);
    }
    
    SaveToFile();
    return true;
}

Todo::DayTasks* TodoManager::GetTasksForDate(const std::string& date) {
    auto it = m_dayTasks.find(date);
    if (it != m_dayTasks.end()) {
        return it->second.get();
    }
    return nullptr;
}

const Todo::DayTasks* TodoManager::GetTasksForDate(const std::string& date) const {
    auto it = m_dayTasks.find(date);
    if (it != m_dayTasks.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<std::shared_ptr<Todo::Task>> TodoManager::GetTasksInDateRange(const std::string& startDate, const std::string& endDate) const {
    std::vector<std::shared_ptr<Todo::Task>> result;
    
    for (const auto& [date, dayTasks] : m_dayTasks) {
        if (date >= startDate && date <= endDate && dayTasks) {
            for (const auto& task : dayTasks->tasks) {
                if (task) {
                    result.push_back(task);
                }
            }
        }
    }
    
    return result;
}

std::vector<std::shared_ptr<Todo::Task>> TodoManager::GetOverdueTasks() const {
    std::vector<std::shared_ptr<Todo::Task>> result;
    
    for (const auto& [date, dayTasks] : m_dayTasks) {
        if (dayTasks) {
            for (const auto& task : dayTasks->tasks) {
                if (task && task->IsOverdue()) {
                    result.push_back(task);
                }
            }
        }
    }
    
    return result;
}

std::vector<std::shared_ptr<Todo::Task>> TodoManager::GetTodayTasks() const {
    std::string today = GetTodayDate();
    const auto* dayTasks = GetTasksForDate(today);
    if (dayTasks) {
        return dayTasks->tasks;
    }
    return {};
}

std::vector<std::shared_ptr<Todo::Task>> TodoManager::GetUpcomingTasks(int days) const {
    std::string today = GetTodayDate();
    std::string endDate = today;
    
    // Calculate end date (today + days)
    auto now = std::chrono::system_clock::now();
    auto futureTime = now + std::chrono::hours(24 * days);
    auto futureTimeT = std::chrono::system_clock::to_time_t(futureTime);
    std::tm* tm_future = std::localtime(&futureTimeT);
    
    char futureStr[32];
    std::strftime(futureStr, sizeof(futureStr), "%Y-%m-%d", tm_future);
    endDate = std::string(futureStr);
    
    return GetTasksInDateRange(today, endDate);
}

void TodoManager::SetCurrentDate(const std::string& date) {
    if (IsValidDate(date)) {
        m_currentDate = date;
        NotifyDayChanged(date);
    }
}

std::string TodoManager::GetTodayDate() const {
    auto now = std::chrono::system_clock::now();
    auto today = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_today = std::localtime(&today);
    
    char todayStr[32];
    std::strftime(todayStr, sizeof(todayStr), "%Y-%m-%d", tm_today);
    
    return std::string(todayStr);
}

std::string TodoManager::GetPreviousDay(const std::string& date) {
    auto tm = ParseDate(date);
    auto timeT = std::mktime(&tm);
    auto prevTime = timeT - 24 * 60 * 60; // Subtract one day
    auto* prevTm = std::localtime(&prevTime);
    
    return FormatDate(*prevTm);
}

std::string TodoManager::GetNextDay(const std::string& date) {
    auto tm = ParseDate(date);
    auto timeT = std::mktime(&tm);
    auto nextTime = timeT + 24 * 60 * 60; // Add one day
    auto* nextTm = std::localtime(&nextTime);
    
    return FormatDate(*nextTm);
}

std::string TodoManager::GetWeekStart(const std::string& date) {
    auto tm = ParseDate(date);
    auto timeT = std::mktime(&tm);
    auto* dateTm = std::localtime(&timeT);
    
    // Calculate days to subtract to get to Monday (assuming Sunday = 0)
    int daysToSubtract = (dateTm->tm_wday == 0) ? 6 : dateTm->tm_wday - 1;
    auto weekStartTime = timeT - (daysToSubtract * 24 * 60 * 60);
    auto* weekStartTm = std::localtime(&weekStartTime);
    
    return FormatDate(*weekStartTm);
}

void TodoManager::StartDrag(std::shared_ptr<Todo::Task> task, const std::string& sourceDate) {
    m_dragDropState.isDragging = true;
    m_dragDropState.draggedTask = task;
    m_dragDropState.sourceDate = sourceDate;
    m_dragDropState.targetDate = "";
    m_dragDropState.targetIndex = -1;
}

void TodoManager::UpdateDrag(const std::string& targetDate, int targetIndex) {
    m_dragDropState.targetDate = targetDate;
    m_dragDropState.targetIndex = targetIndex;
}

void TodoManager::EndDrag() {
    if (m_dragDropState.isDragging && m_dragDropState.draggedTask) {
        if (!m_dragDropState.targetDate.empty() && 
            m_dragDropState.targetDate != m_dragDropState.sourceDate) {
            // Move to different date
            MoveTask(m_dragDropState.draggedTask->id, m_dragDropState.targetDate);
        } else if (m_dragDropState.targetIndex >= 0) {
            // Reorder within same date
            ReorderTask(m_dragDropState.draggedTask->id, m_dragDropState.sourceDate, m_dragDropState.targetIndex);
        }
    }
    
    m_dragDropState = Todo::DragDropState();
}

int TodoManager::GetTotalTaskCount() const {
    int count = 0;
    for (const auto& [date, dayTasks] : m_dayTasks) {
        if (dayTasks) {
            count += static_cast<int>(dayTasks->tasks.size());
        }
    }
    return count;
}

int TodoManager::GetCompletedTaskCount() const {
    int count = 0;
    for (const auto& [date, dayTasks] : m_dayTasks) {
        if (dayTasks) {
            count += dayTasks->GetCompletedCount();
        }
    }
    return count;
}

int TodoManager::GetPendingTaskCount() const {
    return GetTotalTaskCount() - GetCompletedTaskCount();
}

int TodoManager::GetOverdueTaskCount() const {
    return static_cast<int>(GetOverdueTasks().size());
}

float TodoManager::GetCompletionRate() const {
    int total = GetTotalTaskCount();
    if (total == 0) return 0.0f;
    
    int completed = GetCompletedTaskCount();
    return static_cast<float>(completed) / static_cast<float>(total);
}

bool TodoManager::SaveToFile() {
    // TODO: Implement JSON/SQLite persistence
    // For now, just return true
    return true;
}

bool TodoManager::LoadFromFile() {
    // TODO: Implement JSON/SQLite persistence
    // For now, create some sample data
    
    // Create sample tasks for today
    std::string today = GetTodayDate();
    auto task1 = std::make_shared<Todo::Task>("Review project proposal");
    task1->priority = Todo::Priority::High;
    task1->dueDate = today;
    task1->dueTime = "09:00";
    task1->isAllDay = false;
    
    auto task2 = std::make_shared<Todo::Task>("Team standup meeting");
    task2->priority = Todo::Priority::Medium;
    task2->dueDate = today;
    task2->dueTime = "10:30";
    task2->isAllDay = false;
    
    auto task3 = std::make_shared<Todo::Task>("Complete documentation");
    task3->priority = Todo::Priority::Medium;
    task3->dueDate = today;
    
    EnsureDayExists(today);
    m_dayTasks[today]->AddTask(task1);
    m_dayTasks[today]->AddTask(task2);
    m_dayTasks[today]->AddTask(task3);
    
    return true;
}

void TodoManager::EnsureDayExists(const std::string& date) {
    if (m_dayTasks.find(date) == m_dayTasks.end()) {
        m_dayTasks[date] = std::make_unique<Todo::DayTasks>();
        m_dayTasks[date]->date = date;
    }
}

std::string TodoManager::GetDataFilePath() const {
    // TODO: Get from config or use default location
    return "todos.json";
}

void TodoManager::NotifyTaskUpdated(std::shared_ptr<Todo::Task> task) {
    if (m_onTaskUpdated) {
        m_onTaskUpdated(task);
    }
}

void TodoManager::NotifyTaskCompleted(std::shared_ptr<Todo::Task> task) {
    if (m_onTaskCompleted) {
        m_onTaskCompleted(task);
    }
}

void TodoManager::NotifyDayChanged(const std::string& date) {
    if (m_onDayChanged) {
        m_onDayChanged(date);
    }
}

bool TodoManager::IsValidDate(const std::string& date) const {
    // Basic validation for YYYY-MM-DD format
    if (date.length() != 10) return false;
    if (date[4] != '-' || date[7] != '-') return false;
    
    // Try to parse
    std::istringstream ss(date);
    std::tm tm = {};
    ss >> std::get_time(&tm, "%Y-%m-%d");
    
    return !ss.fail();
}

std::tm TodoManager::ParseDate(const std::string& date) const {
    std::istringstream ss(date);
    std::tm tm = {};
    ss >> std::get_time(&tm, "%Y-%m-%d");
    return tm;
}

std::string TodoManager::FormatDate(const std::tm& date) const {
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &date);
    return std::string(buffer);
}