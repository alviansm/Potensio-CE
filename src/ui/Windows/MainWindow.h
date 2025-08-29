// MainWindow.h - Updated with Settings Module
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <fstream>
#include <cstdlib>

#include "imgui.h"

// Modules
#include "core/Kanban/KanbanManager.h"
#include "core/Todo/TodoManager.h"
#include "core/Clipboard/ClipboardManager.h"
#include "core/FileConverter/FileConverter.h"
#include "core/Timer/PomodoroTimer.h"

// Utilities
#include "core/Utils.h"

// Forward declarations
class Sidebar;
class PomodoroWindow;
class KanbanManager;
class KanbanWindow;
class TodoManager;
class ClipboardManager;
class ClipboardWindow;
class AppConfig;
class UIManager;

class DatabaseManager;
class PomodoroDatabase;
class KanbanDatabase;
class TodoDatabase;

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

enum class SettingsCategory
{
    General = 0,
    Appearance,
    Hotkeys,
    Modules,
    Account,
    About
};

// File item structure for the dropover interface
struct FileItem
{
    std::string name;
    std::string fullPath;
    std::string size;
    std::string type;
    std::string modified;
    bool isDirectory;
    
    FileItem(const std::string& path) : fullPath(path)
    {
        name = Utils::GetFileName(path);
        isDirectory = Utils::DirectoryExists(path);
        
        if (!isDirectory)
        {
            uint64_t sizeBytes = Utils::GetFileSize(path);
            size = Utils::FormatBytes(sizeBytes);
            type = Utils::GetFileExtension(path);
        }
        else
        {
            size = "Folder";
            type = "Folder";
        }
        
        modified = "Recently"; // Simplified for now
    }
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
    void InitializeResources();

    static ImTextureID LoadTextureFromFile(const char* filename, int* out_width = nullptr, int* out_height = nullptr);
    static void UnloadTexture(ImTextureID tex_id);
    static unsigned char* LoadPNGFromResource(int resourceID, int* out_width, int* out_height);
    static ImTextureID LoadTextureFromResource(int resourceID);

    /**
     * @note API for file staging module
     */
    void AddStagedFile(const std::string& path);
    const std::vector<FileItem>& GetStagedFiles() const;

private:
    std::unique_ptr<Sidebar> m_sidebar;
    ModulePage m_currentModule = ModulePage::Dashboard;
    bool m_alwaysOnTop = false;
    AppConfig* m_config = nullptr;

    bool m_shortcutsEnabled = true;

    float m_pasteProgress = 0.0f;
    bool m_pasteInProgress = false;
    float m_cutProgress = 0.0f;
    bool m_cutInProgress = false;
    bool showBulkRenamePopup = false;
    char renameBuffer[256] = "";
    std::string renameError;

    // File staging
    std::vector<FileItem> m_stagedFiles;

    // File staging utilities
    std::string BrowseForFolder(HWND hwndOwner = nullptr);
    bool CopyFileWithProgress(const std::filesystem::path &src,
                              const std::filesystem::path &dst,
                              std::function<void(float)> progressCallback);
    bool MoveFileOrDirWithProgress(const std::filesystem::path &src,
                                   const std::filesystem::path &dst,
                                   std::function<void(float)> progressCallback);
    bool IsCriticalFileOrDir(const std::string &path);
    bool IsSystemPath(const std::string &path);
    bool IsProtectedFile(const std::string &path);
    void MoveToRecycleBin(const std::vector<std::filesystem::path> &files);

    // File staging context menu
    void OpenFile(const std::string &path);
    void ShowInExplorer(const std::string &path);
    
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
        Clipboard::ClipboardFormat filterFormat = Clipboard::ClipboardFormat::Text;
        int selectedItemIndex = -1;
        bool showPreview = true;
    } m_clipboardUIState;

    // File Converter integration
    std::unique_ptr<FileConverter> m_fileConverter;
    bool m_showFileConverterSettings = false;
    
    // File converter UI state
    struct FileConverterUIState
    {
        bool isDragOver = false;
        std::vector<std::string> draggedFiles;
        FileType outputFormat = FileType::JPG;
        int imageQuality = 85;
        int pngCompression = 6;
        bool preserveMetadata = false;
        size_t targetSizeKB = 0;
        std::string outputDirectory;
        bool useSourceDirectory = true;
        int selectedJobIndex = -1;
        bool showCompletedJobs = true;
        bool autoProcessJobs = true;
        std::string searchFilter;
    } m_fileConverterUIState;

    // Settings UI state
    struct SettingsUIState
    {
        SettingsCategory selectedCategory = SettingsCategory::General;
        
        // General settings
        bool startWithWindows = false;
        bool startMinimized = false;
        bool minimizeToTray = true;
        bool closeToTray = true;
        bool showNotifications = true;
        bool enableSounds = true;
        bool autoSave = true;
        int autoSaveInterval = 30; // seconds
        
        // Appearance settings
        int themeMode = 0; // 0=Dark, 1=Light, 2=Auto
        float uiScale = 1.0f;
        int accentColor = 0; // Color scheme index
        bool enableAnimations = true;
        bool compactMode = false;
        
        // Hotkey settings
        char globalHotkeyBuffer[64] = "Ctrl+Shift+Q";
        char pomodoroStartBuffer[64] = "Ctrl+Alt+P";
        char quickCaptureBuffer[64] = "Ctrl+Shift+C";
        char showTodayTasksBuffer[64] = "Ctrl+Shift+T";
        
        // Module settings
        bool enablePomodoro = true;
        bool enableKanban = true;
        bool enableTodo = true;
        bool enableClipboard = true;
        bool enableFileConverter = true;
        bool enableDropover = true;
        
        // Account settings (placeholders for future)
        char usernameBuffer[128] = "";
        char emailBuffer[256] = "";
        bool syncEnabled = false;
        bool cloudBackup = false;
        
        // Update settings
        bool autoCheckUpdates = true;
        bool downloadUpdatesAuto = false;
        bool betaChannel = false;
        std::string currentVersion = "0.1.0";
        std::string latestVersion = "";
        bool updateAvailable = false;
        bool checkingUpdates = false;
        
    } m_settingsUIState;
    
    // Settings dialog states
    bool m_showResetConfirmDialog = false;
    bool m_showExportDataDialog = false;
    bool m_showImportDataDialog = false;
    bool m_showClearDataDialog = false;
    
    // Content area rendering
    void RenderMenuBar();
    void RenderExitPopup();
    void RenderAboutPopup();
    void RenderContentArea();

    // Settings Module - Main Interface
    void RenderSettingsModule();
    void RenderSettingsSidebar();
    void RenderSettingsContent();
    
    // Settings Categories
    void RenderGeneralSettings();
    void RenderAppearanceSettings();
    void RenderHotkeySettings();
    void RenderModuleSettings();
    void RenderAccountSettings();
    void RenderAboutSettings();
    
    // Settings Dialogs
    void RenderResetConfirmDialog();
    void RenderDataManagementDialogs();
    void RenderHotkeyEditor(const char* label, char* buffer, size_t bufferSize);
    
    // Settings Helpers
    void LoadSettingsFromConfig();
    void SaveSettingsToConfig();
    void ResetSettingsToDefaults();
    void ApplyThemeSettings();
    void CheckForUpdates();
    void ExportUserData();
    void ImportUserData();
    void ClearAllUserData();
    const char* GetThemeModeName(int mode) const;
    const char* GetCategoryName(SettingsCategory category) const;
    ImVec4 GetAccentColor(int colorIndex) const;

    // Dashboard
    void RenderDropoverInterface();
    void RenderDropoverToolbar();
    void RenderDropoverArea();
    void RenderFileList();
    
    // Pomodoro - Main Interface
    void RenderPomodoroModule();
    void RenderPomodoroTimer();
    void RenderPomodoroControls();
    void RenderPomodoroProgress();
    void RenderPomodoroSessionInfo();
    void RenderPomodoroQuickSettings();
    void RenderPomodoroNotifications();
    
    // Pomodoro helpers
    void OnPomodoroSessionComplete(int sessionType);
    void OnPomodoroAllComplete();
    void OnPomodoroTick();
    
    // Kanban - Main Interface
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
    
    // Todo - Main Interface
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
        int selectedMonth = 8;
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

    // Clipboard - Main Interface
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
    
    // Other modules
    void RenderBulkRenamePlaceholder();

    // FileConverter - Main Interface
    void RenderFileConverterModule();
    void RenderFileConverterHeader();
    void RenderFileConverterDropZone();
    void RenderFileConverterSettings();
    void RenderFileConverterJobQueue();
    void RenderFileConverterJob(std::shared_ptr<FileConversionJob> job, int index, bool isSelected);
    void RenderFileConverterProgress();
    void RenderFileConverterStats();
    
    // File Converter helpers
    void OnFileConverterJobProgress(const std::string& jobId, float progress);
    void OnFileConverterJobComplete(const std::string& jobId, bool success, const std::string& error);
    void ProcessDroppedFiles(const std::vector<std::string>& files);
    void AddConversionJob(const std::string& inputPath);
    std::string GenerateOutputPath(const std::string& inputPath, FileType outputType);
    bool IsFileSupported(const std::string& path);
    
    // Utility
    const char* GetModuleName(ModulePage module) const;
    
    // Priority and status color helpers
    ImVec4 GetPriorityColor(int priority) const;
    const char* GetPriorityName(int priority) const;
    const char* GetStatusName(int status) const;
    ImVec4 GetStatusColor(int status) const;

private:
    UIManager* m_uiManager;

    // Database members
    std::shared_ptr<DatabaseManager> m_databaseManager;
    std::shared_ptr<PomodoroDatabase> m_pomodoroDatabase;
    std::shared_ptr<KanbanDatabase> m_kanbanDatabase;

    // Change listener
    bool m_kanbanChanged = false;
    
    // Settings management
    bool m_openBoardPopup = false;
    char m_boardNameBuffer[128] = ""; // buffer for input
    std::string m_errorMessageBoard;

    bool m_openCreateProjectPopup = false;
    char m_projectNameBuffer[128] = "";
    bool m_nameExistsError = false;

    bool m_exitPopupOpen = false;
    bool m_showAboutDialog = false;

    std::string m_cardToDeleteId;
    bool m_confirmDeleteCardPopup = false;

    // Current active session tracking
    int m_currentSessionId = -1;
    
    // Helper methods
    bool InitializeDatabase();
    std::string GetDatabasePath();
    void LoadPomodoroConfiguration();
    void SavePomodoroConfiguration(const PomodoroTimer::PomodoroConfig& config);
    void OnPomodoroSessionStart();

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

    // Additional icons for Pomodoro modules
    ImTextureID iconPlay = nullptr;
    ImTextureID iconPause = nullptr;
    ImTextureID iconReset = nullptr;
    ImTextureID iconSkip = nullptr;
    ImTextureID iconStop = nullptr;

    // Additional icons for Kanban modules
    ImTextureID iconEditKanban = nullptr;
    ImTextureID iconDeleteKanban = nullptr;
    ImTextureID iconDueDateKanban = nullptr;
    ImTextureID iconPriorityKanban = nullptr;
    ImTextureID iconSettingKanban = nullptr;
    ImTextureID iconPriorityLowKanban = nullptr;
    ImTextureID iconPriorityMediumKanban = nullptr;
    ImTextureID iconPriorityHighKanban = nullptr;
    ImTextureID iconPriorityUrgentKanban = nullptr;
};