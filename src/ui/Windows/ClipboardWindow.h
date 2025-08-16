// ui/Windows/ClipboardWindow.h
#pragma once

#include <memory>
#include <string>

// Forward declarations
class ClipboardManager;
class AppConfig;

class ClipboardWindow
{
public:
    ClipboardWindow();
    ~ClipboardWindow();

    // Initialization
    bool Initialize(AppConfig* config);
    void Shutdown();

    // Update and render
    void Update(float deltaTime);
    void Render();

    // Window management
    void SetVisible(bool visible) { m_isVisible = visible; }
    bool IsVisible() const { return m_isVisible; }

    // Manager connection
    void SetClipboardManager(ClipboardManager* manager) { m_clipboardManager = manager; }

private:
    AppConfig* m_config = nullptr;
    ClipboardManager* m_clipboardManager = nullptr;
    bool m_isVisible = false;
    bool m_isInitialized = false;

    // UI state
    struct UIState
    {
        // General settings
        bool enableMonitoring = true;
        int maxHistorySize = 100;
        int maxItemSizeKB = 1024;
        bool autoCleanup = true;
        int autoCleanupDays = 30;
        
        // Content settings
        bool saveImages = true;
        bool saveFiles = true;
        bool saveRichText = true;
        
        // Interface settings
        bool showNotifications = true;
        bool enableHotkeys = true;
        bool monitorWhenHidden = true;
        
        // Exclude apps
        char excludeAppsBuffer[512] = "";
        
        // Import/Export
        char exportPathBuffer[512] = "";
        char importPathBuffer[512] = "";
        
        // Statistics display
        bool showAdvancedStats = false;
        
    } m_uiState;

    // UI rendering methods
    void RenderGeneralSettings();
    void RenderContentSettings();
    void RenderInterfaceSettings();
    void RenderExcludeApps();
    void RenderStatistics();
    void RenderImportExport();
    void RenderAdvancedSettings();

    // Helper methods
    void LoadUIState();
    void SaveUIState();
    void ApplySettings();
    void ResetToDefaults();
    
    // File dialog helpers
    std::string ShowSaveFileDialog(const std::string& title, const std::string& filter);
    std::string ShowOpenFileDialog(const std::string& title, const std::string& filter);
    
    // Validation - MAKE THIS CONST
    bool ValidateSettings() const;
    void ShowValidationError(const std::string& message) const; // <-- Add const here
};