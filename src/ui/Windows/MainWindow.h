// MainWindow.h - Updated with Pomodoro Integration
#pragma once

#include <memory>
#include <string>

#include "imgui.h"

// Forward declarations
class Sidebar;
class PomodoroTimer;
class PomodoroWindow;
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
     * @note Other modules
     */
    void RenderKanbanPlaceholder();
    void RenderClipboardPlaceholder();
    void RenderBulkRenamePlaceholder();
    void RenderFileConverterPlaceholder();
    void renderSettingPlaceholder();
    
    // Utility
    const char* GetModuleName(ModulePage module) const;

private:
    // Icon textures
    ImTextureID iconPaste   = nullptr;
    ImTextureID iconCut     = nullptr;
    ImTextureID iconTrash   = nullptr;
    ImTextureID iconRename  = nullptr;
    ImTextureID iconClear   = nullptr;
    ImTextureID iconPin     = nullptr;
    ImTextureID iconTrigger = nullptr;
};