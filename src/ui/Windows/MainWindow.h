// MainWindow.h - Updated with Todo and Kanban Integration
#pragma once

#include <memory>
#include <string>

#include "imgui.h"

// Modules
#include "core/Kanban/KanbanManager.h"
#include "core/Todo/TodoManager.h"
#include "core/Clipboard/ClipboardManager.h"

// Forward declarations
class Sidebar;
class PomodoroTimer;
class PomodoroWindow;
class KanbanManager;
class KanbanWindow;
class TodoManager;
class ClipboardManager;
class ClipboardWindow;
class AppConfig;

enum class ModulePage
{
    Dashboard = 0,
    Pomodoro,
    Kanban,
    Todo,
    Clipboard,
    FileConverter,
    Settings
};

class MainWindow
{
public:
    MainWindow();
    ~MainWindow();

    // Initialization
    bool Initialize(AppConfig* config);
    void Shutdown();

    // Update and render
    void Update(float deltaTime);
    void Render();
    
    // Module navigation
    void SetCurrentModule(ModulePage module);
    ModulePage GetCurrentModule() const { return m_currentModule; }

    // Load Textures
    void InitializeResources(); // Main load texture function

    static ImTextureID LoadTextureFromFile(const char* filename, int* out_width = nullptr, int* out_height = nullptr);
    static void UnloadTexture(ImTextureID tex_id);
    static unsigned char* LoadPNGFromResource(int resourceID, int* out_width, int* out_height);
    static ImTextureID LoadTextureFromResource(int resourceID);

private:
    std::unique_ptr<Sidebar> m_sidebar;
    ModulePage m_currentModule = ModulePage::Dashboard;
    bool m_alwaysOnTop = false;
    AppConfig* m_config = nullptr;
    
    // Pomodoro integration
    std::unique_ptr<PomodoroTimer> m_pomodoroTimer;
    std::unique_ptr<PomodoroWindow> m_pomodoroSettingsWindow;
    bool m_showPomodoroSettings = false;
    
    // Kanban integration
    std::unique_ptr<KanbanManager> m_kanbanManager;
    std::unique_ptr<KanbanWindow> m_kanbanSettingsWindow;
    bool m_showKanbanSettings = false;
    
    // Todo integration
    std::unique_ptr<TodoManager> m_todoManager;
    bool m_showTodoSettings = false;

    // Clipboard integration
    std::unique_ptr<ClipboardManager> m_clipboardManager;
    std::unique_ptr<ClipboardWindow> m_clipboardSettingsWindow;
    bool m_showClipboardSettings = false;
    
    // Card editing state (for Kanban)
    struct CardEditState
    {
        bool isEditing = false;
        std::string cardId;
        char titleBuffer[256] = "";
        char descriptionBuffer[1024] = "";
        int priority = 0;
        char dueDateBuffer[32] = "";
        char assigneeBuffer[128] = "";
    } m_cardEditState;
    
    // Task editing state (for Todo)
    struct TaskEditState
    {
        bool isEditing = false;
        std::string taskId;
        char titleBuffer[256] = "";
        char descriptionBuffer[1024] = "";
        int priority = 0;
        int status = 0;
        char dueDateBuffer[32] = "";
        char dueTimeBuffer[16] = "";
        bool isAllDay = true;
        char categoryBuffer[128] = "";
    } m_taskEditState;

    // Clipboard UI state
    struct ClipboardUIState
    {
        std::string searchQuery;
        bool showFavorites = false;
        bool showPinned = false;
        Clipboard::ClipboardFormat filterFormat = Clipboard::ClipboardFormat::Text; // "All" filter
        int selectedItemIndex = -1;
        bool showPreview = true;
    } m_clipboardUIState;
    
    // Content area rendering
    void RenderMenuBar();
    void RenderContentArea();

    /**
     * @note Dashboard
     */
    void RenderDropoverInterface();
    void RenderDropoverToolbar();
    void RenderDropoverArea();
    void RenderFileList();
    
    /**
     * @note Pomodoro - Main Interface
     */
    void RenderPomodoroModule();
    void RenderPomodoroTimer();
    void RenderPomodoroControls();
    void RenderPomodoroProgress();
    void RenderPomodoroSessionInfo();
    void RenderPomodoroQuickSettings();
    
    // Pomodoro helpers
    void OnPomodoroSessionComplete(int sessionType);
    void OnPomodoroAllComplete();
    void OnPomodoroTick();
    
    /**
     * @note Kanban - Main Interface
     */
    void RenderKanbanModule();
    void RenderKanbanHeader();
    void RenderKanbanBoard();
    void RenderKanbanColumn(class Kanban::Column* column, int columnIndex, float columnWidth, float columnHeight);
    void RenderQuickAddCard(const std::string& columnId, float maxWidth = 0.0f);
    void RenderKanbanCard(std::shared_ptr<class Kanban::Card> card, int cardIndex, const std::string& columnId);
    void RenderCardEditDialog();
    
    // Kanban helpers
    void StartEditingCard(std::shared_ptr<class Kanban::Card> card);
    void StopEditingCard(bool save = false);
    void OnKanbanCardUpdated(std::shared_ptr<class Kanban::Card> card);
    void OnKanbanBoardChanged(class Kanban::Board* board);
    void OnKanbanProjectChanged(class Kanban::Project* project);
    
    // Kanban drag and drop
    void HandleCardDragDrop(std::shared_ptr<class Kanban::Card> card, const std::string& columnId, int cardIndex);
    void RenderDropTarget(const std::string& columnId, int insertIndex = -1);
    
    /**
     * @note Todo - Main Interface
     */
    void RenderTodoModule();
    void RenderTodoHeader();
    void RenderTodoNavigation();
    void RenderTodoCalendarView();
    void RenderTodoDailyView();
    void RenderTodoWeeklyView();
    void RenderTodoTaskList(const std::string& date, const std::vector<std::shared_ptr<Todo::Task>>& tasks);
    void RenderTodoTask(std::shared_ptr<Todo::Task> task, int taskIndex, const std::string& date);
    void RenderQuickAddTask(const std::string& date);
    void RenderTaskEditDialog();
    void RenderTodoSidebar();
    
    // Todo helpers
    void StartEditingTask(std::shared_ptr<Todo::Task> task);
    void StopEditingTask(bool save = false);
    void OnTodoTaskUpdated(std::shared_ptr<Todo::Task> task);
    void OnTodoTaskCompleted(std::shared_ptr<Todo::Task> task);
    void OnTodoDayChanged(const std::string& date);
    
    // Todo drag and drop
    void RenderTodoDropTarget(const std::string& date, int insertIndex = -1);

    // Todo date picker state
    struct DatePickerState
    {
        bool isOpen = false;
        int selectedYear = 2025;
        int selectedMonth = 8;  // 0-based (0=January, 7=August)
        int selectedDay = 15;
        int displayYear = 2025;
        int displayMonth = 8;
    } m_datePickerState;
    
    // Date picker helper methods
    void GetCurrentDateComponents(int& year, int& month, int& day) const;
    std::string FormatDateComponents(int year, int month, int day) const;
    int GetDaysInMonth(int year, int month) const;
    int GetFirstDayOfWeek(int year, int month) const;
    void RenderDatePicker();

    /**
     * @note Clipboard - Main Interface
     */
    void RenderClipboardModule();
    void RenderClipboardHeader();
    void RenderClipboardToolbar();
    void RenderClipboardList();
    void RenderClipboardItem(std::shared_ptr<Clipboard::ClipboardItem> item, int index, bool isSelected);
    void RenderClipboardPreview();
    void RenderClipboardSearch();

    // Clipboard helpers
    void OnClipboardItemAdded(std::shared_ptr<Clipboard::ClipboardItem> item);
    void OnClipboardItemDeleted(const std::string& id);
    void OnClipboardHistoryCleared();

    // Clipboard operations
    void CopyClipboardItem(std::shared_ptr<Clipboard::ClipboardItem> item);
    void DeleteClipboardItem(const std::string& id);
    void ToggleClipboardFavorite(const std::string& id);
    void ClearClipboardHistory();

    // Clipboard utility methods
    const char* GetClipboardFormatName(Clipboard::ClipboardFormat format) const;
    ImVec4 GetClipboardFormatColor(Clipboard::ClipboardFormat format) const;
    std::string FormatClipboardTimestamp(const std::chrono::system_clock::time_point& timestamp) const;
    
    /**
     * @note Other modules
     */
    void RenderBulkRenamePlaceholder();
    void RenderFileConverterPlaceholder();
    void renderSettingPlaceholder();
    
    // Utility
    const char* GetModuleName(ModulePage module) const;
    
    // Priority and status color helpers
    ImVec4 GetPriorityColor(int priority) const;
    const char* GetPriorityName(int priority) const;
    const char* GetStatusName(int status) const;
    ImVec4 GetStatusColor(int status) const;

private:
    // Icon textures
    ImTextureID iconPaste   = nullptr;
    ImTextureID iconCut     = nullptr;
    ImTextureID iconTrash   = nullptr;
    ImTextureID iconRename  = nullptr;
    ImTextureID iconClear   = nullptr;
    ImTextureID iconPin     = nullptr;
    ImTextureID iconTrigger = nullptr;
    
    // Additional icons for modules
    ImTextureID iconAdd     = nullptr;
    ImTextureID iconEdit    = nullptr;
    ImTextureID iconMove    = nullptr;
    ImTextureID iconSettings = nullptr;
    ImTextureID iconCalendar = nullptr;
    ImTextureID iconTask    = nullptr;
};