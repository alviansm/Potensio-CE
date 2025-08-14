// MainWindow.h - FIXED VERSION
#pragma once

#include <memory>
#include <string>

#include "imgui.h"

// Forward declarations
class Sidebar;

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

    // Update and render
    void Update(float deltaTime);
    void Render();
    
    // Module navigation
    void SetCurrentModule(ModulePage module);
    ModulePage GetCurrentModule() const { return m_currentModule; }

    // Load Textures
    void InitializeResources(); // Main load teture function

    static ImTextureID LoadTextureFromFile(const char* filename, int* out_width = nullptr, int* out_height = nullptr);
    static void UnloadTexture(ImTextureID tex_id);
    static unsigned char* LoadPNGFromResource(int resourceID, int* out_width, int* out_height);
    static ImTextureID LoadTextureFromResource(int resourceID);

private:
    std::unique_ptr<Sidebar> m_sidebar;
    ModulePage m_currentModule = ModulePage::Dashboard;
    bool m_alwaysOnTop = false;  // New member for always on top state
    
    // Content area rendering
    void RenderMenuBar();               // New menubar rendering

    void RenderContentArea();

    /**
     * @note Dashboard
     */
    void RenderDropoverInterface();     // New dropover interface
    void RenderDropoverToolbar();       // Toolbar with file operations
    void RenderDropoverArea();          // Main drop zone and file list
    void RenderFileList();              // File list table    
    
    /**
     * @note Pomodoro
     */
    void RenderPomodoroPlaceholder();
    void RenderKanbanPlaceholder();
    void RenderClipboardPlaceholder();
    void RenderBulkRenamePlaceholder();
    void RenderFileConverterPlaceholder();

    void renderSettingPlaceholder();
    
    // Utility
    const char* GetModuleName(ModulePage module) const;

private:
    ImTextureID iconPaste   = nullptr;
    ImTextureID iconCut     = nullptr;
    ImTextureID iconTrash   = nullptr;
    ImTextureID iconRename  = nullptr;
    ImTextureID iconClear   = nullptr;
    ImTextureID iconPin   = nullptr;
    ImTextureID iconTrigger   = nullptr;
};