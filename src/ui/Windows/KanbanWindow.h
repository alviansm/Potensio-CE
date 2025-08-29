#pragma once

#include "core/Kanban/KanbanManager.h"
#include <memory>
#include <string>

class AppConfig;

// KanbanWindow for advanced settings and project/board management
// Main kanban interface is handled by MainWindow
class KanbanWindow
{
public:
    KanbanWindow();
    ~KanbanWindow() = default;

    // Initialization
    bool Initialize(AppConfig* config);
    void Shutdown();

    // Main update and rendering (for settings dialog)
    void Update(float deltaTime);
    void Render();
    
    // Window state
    bool IsVisible() const { return m_isVisible; }
    void SetVisible(bool visible) { m_isVisible = visible; }
    
    // Configuration management
    void SetKanbanManager(KanbanManager* manager) { m_kanbanManager = manager; }
    
    // Open specific settings panels
    void ShowProjectManagement() { m_isVisible = true; m_showProjectManagement = true; }
    void ShowBoardSettings() { m_isVisible = true; m_showBoardSettings = true; }
    void ShowAppearanceSettings() { m_isVisible = true; m_showAppearanceSettings = true; }
    void ShowImportExport() { m_isVisible = true; m_showImportExport = true; }

private:
    void RenderSettingsWindow();
    void RenderProjectManagement();
    void RenderBoardSettings();
    void RenderAppearanceSettings();
    void RenderImportExport();
    void RenderStatistics();
    
    // Project/Board management helpers
    void RenderProjectList();
    void RenderBoardList();
    void RenderColumnEditor();
    void RenderCardTemplates();
    
    // UI helpers
    void RenderColorPicker(const char* label, Kanban::Color& color);
    void RenderPrioritySelector(const char* label, Kanban::Priority& priority);
    bool RenderConfirmationDialog(const char* title, const char* message, bool* modal = nullptr);
    
    // Configuration
    void LoadSettings();
    void SaveSettings();
    void ResetToDefaults();
    
    // File operations
    void OpenSaveProjectDialog();
    void OpenLoadProjectDialog();
    void OpenExportBoardDialog();

private:
    KanbanManager* m_kanbanManager; // Reference to manager (not owned)
    AppConfig* m_config;
    
    // UI state
    bool m_isVisible;
    bool m_showProjectManagement;
    bool m_showBoardSettings;
    bool m_showAppearanceSettings;
    bool m_showImportExport;
    bool m_showStatistics;
    
    // Settings state
    bool m_settingsChanged;
    
    // Temporary data for editing
    struct TempProjectData
    {
        std::string name;
        std::string description;
        bool isValid = false;
    } m_tempProject;
    
    struct TempBoardData
    {
        std::string name;
        std::string description;
        std::vector<std::string> columnNames;
        bool isValid = false;
    } m_tempBoard;
    
    struct TempColumnData
    {
        std::string name;
        Kanban::Color headerColor;
        int cardLimit = -1;
        bool isValid = false;
    } m_tempColumn;
    
    // Dialog states
    struct DialogStates
    {
        bool showNewProjectDialog = false;
        bool showEditProjectDialog = false;
        bool showDeleteProjectConfirm = false;
        bool showNewBoardDialog = false;
        bool showEditBoardDialog = false;
        bool showDeleteBoardConfirm = false;
        bool showNewColumnDialog = false;
        bool showEditColumnDialog = false;
        bool showDeleteColumnConfirm = false;
        
        std::string targetId; // For delete confirmations
        std::string targetName; // For display in confirmations
    } m_dialogs;
    
    // Appearance settings
    struct AppearanceSettings
    {
        Kanban::Color defaultCardColor{0.9f, 0.9f, 0.9f};
        Kanban::Color defaultColumnColor{0.7f, 0.7f, 0.8f};
        float cardRounding = 5.0f;
        float columnRounding = 8.0f;
        float cardSpacing = 8.0f;
        float columnSpacing = 16.0f;
        bool showCardNumbers = true;
        bool showDueDates = true;
        bool enableDragDropAnimation = true;
        bool compactMode = false;
    } m_appearanceSettings;
    
    // Window sizing
    static constexpr float SETTINGS_WINDOW_WIDTH = 700.0f;
    static constexpr float SETTINGS_WINDOW_HEIGHT = 600.0f;
    static constexpr float DIALOG_WIDTH = 400.0f;
    static constexpr float DIALOG_HEIGHT = 300.0f;
};