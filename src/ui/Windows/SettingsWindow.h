#pragma once

#include <string>

class SettingsWindow
{
public:
    SettingsWindow();
    ~SettingsWindow();

    // Update and render
    void Update(float deltaTime);
    void Render();
    
    // Visibility
    void Show() { m_isVisible = true; }
    void Hide() { m_isVisible = false; }
    bool IsVisible() const { return m_isVisible; }

private:
    bool m_isVisible = false;
    
    // Settings categories
    enum class SettingsCategory
    {
        General = 0,
        Appearance,
        Hotkeys,
        About
    };
    
    SettingsCategory m_selectedCategory = SettingsCategory::General;
    
    // Temporary settings (before applying)
    struct TempSettings
    {
        bool startWithWindows = false;
        bool minimizeToTray = true;
        bool showNotifications = true;
        float fontSize = 13.0f;
        int sidebarWidth = 180;
        std::string theme = "dark";
        
        // Hotkey settings
        bool hotkeyEnabled = true;
        int hotkeyModifiers = 6; // Ctrl+Shift
        int hotkeyKey = 80; // P
    } m_tempSettings;
    
    // UI State
    bool m_settingsChanged = false;
    
    // Rendering methods
    void RenderCategorySelector();
    void RenderGeneralSettings();
    void RenderAppearanceSettings();
    void RenderHotkeySettings();
    void RenderAboutSection();
    void RenderButtons();
    
    // Settings management
    void LoadCurrentSettings();
    void ApplySettings();
    void ResetToDefaults();
    bool HasUnsavedChanges() const;
    
    // Utility
    const char* GetCategoryName(SettingsCategory category) const;
};