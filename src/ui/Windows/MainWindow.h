// MainWindow.h - Updated with Kanban Integration
#pragma once

#include <memory>
#include <string>

#include "imgui.h"
#include "core/Kanban/KanbanManager.h"

// Forward declarations
class Sidebar;
class PomodoroTimer;
class PomodoroWindow;
class KanbanManager;
class KanbanWindow;
class AppConfig;

enum class ModulePage
{
    Dashboard = 0,
    Pomodoro,
    Kanban,
    Todo,
    Clipboard,
    Dropover,
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
    
    // Card editing state
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
    void RenderKanbanColumn(class Kanban::Column* column, int columnIndex);
    void RenderKanbanCard(std::shared_ptr<class Kanban::Card> card, int cardIndex, const std::string& columnId);
    void RenderCardEditDialog();
    void RenderQuickAddCard(const std::string& columnId);
    
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
     * @note Other modules
     */
    void RenderClipboardPlaceholder();
    void RenderBulkRenamePlaceholder();
    void RenderFileConverterPlaceholder();
    void renderSettingPlaceholder();
    
    // Utility
    const char* GetModuleName(ModulePage module) const;
    
    // Priority color helpers
    ImVec4 GetPriorityColor(int priority) const;
    const char* GetPriorityName(int priority) const;

private:
    // Icon textures
    ImTextureID iconPaste   = nullptr;
    ImTextureID iconCut     = nullptr;
    ImTextureID iconTrash   = nullptr;
    ImTextureID iconRename  = nullptr;
    ImTextureID iconClear   = nullptr;
    ImTextureID iconPin     = nullptr;
    ImTextureID iconTrigger = nullptr;
    
    // Kanban-specific icons (you can add these to your resources)
    ImTextureID iconAdd     = nullptr;
    ImTextureID iconEdit    = nullptr;
    ImTextureID iconMove    = nullptr;
    ImTextureID iconSettings = nullptr;
};