#pragma once

#include "core/Timer/PomodoroTimer.h"
#include <memory>
#include <string>

class AppConfig;

// Simplified PomodoroWindow for advanced settings and configuration only
// Main timer interface is now handled by MainWindow
class PomodoroWindow
{
public:
    PomodoroWindow();
    ~PomodoroWindow() = default;

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
    void SetTimer(PomodoroTimer* timer) { m_timer = timer; }
    
    // Open specific settings panels
    void ShowAdvancedSettings() { m_isVisible = true; m_showAdvancedSettings = true; }
    void ShowColorSettings() { m_isVisible = true; m_showColorSettings = true; }
    void ShowStatistics() { m_isVisible = true; m_showStatistics = true; }

private:
    void RenderSettingsWindow();
    void RenderAdvancedSettings();
    void RenderColorSettings();
    void RenderStatistics();
    void RenderNotificationSettings();
    
    // Settings UI helpers
    void RenderDurationSettings();
    void RenderSessionSettings();
    void RenderColorPicker(const char* label, PomodoroTimer::PomodoroConfig::ColorRange& color);
    
    // Configuration
    void LoadSettings();
    void SaveSettings();
    void ResetToDefaults();

private:
    PomodoroTimer* m_timer; // Reference to timer (not owned)
    AppConfig* m_config;
    
    // UI state
    bool m_isVisible;
    bool m_showAdvancedSettings;
    bool m_showColorSettings;
    bool m_showStatistics;
    bool m_showNotificationSettings;
    
    // Settings panel state
    PomodoroTimer::PomodoroConfig m_tempConfig;
    bool m_settingsChanged;
    
    // Window sizing
    static constexpr float SETTINGS_WINDOW_WIDTH = 500.0f;
    static constexpr float SETTINGS_WINDOW_HEIGHT = 600.0f;
};