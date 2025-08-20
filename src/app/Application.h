#pragma once

#include <windows.h>
#include <memory>
#include <string>

#define WM_SHOW_POTENSIO (WM_USER + 1)

// Forward declarations
class UIManager;
class SystemTray;
class AppConfig;
class MouseTracker;


class Application
{
public:
    Application();
    ~Application();

    // Initialize the application
    bool Initialize(HINSTANCE hInstance);
    
    // Main application loop
    int Run();
    
    // Shutdown the application
    void Shutdown();
    
    // Show/hide main window
    void ShowMainWindow();
    void HideMainWindow();
    void ToggleMainWindow();
    
    // Get singleton instance
    static Application* GetInstance() { return s_instance; }
    
    // Check if application should exit
    bool ShouldExit() const { return m_shouldExit; }
    
    // Request application exit
    void RequestExit() { m_shouldExit = true; }
    
    // Get application instance handle
    HINSTANCE GetInstanceHandle() const { return m_hInstance; }
    
    // Get application configuration
    AppConfig* GetConfig() const { return m_config.get(); }

    // Settings integration
    void ApplyStartupSettings();
    void RegisterStartupWithWindows(bool enable);
    void UpdateHotkeys();
    void ApplySystemSettings();
    void UpdateSettingsFromConfig();
    
    // Hotkey callback registration
    void RegisterSettingsHotkeys();
    void UnregisterSettingsHotkeys();

private:
    // Static instance for singleton access
    static Application* s_instance;

    static HHOOK g_mouseHook;
    
    // Application state
    HINSTANCE m_hInstance = nullptr;
    bool m_shouldExit = false;
    bool m_isVisible = false;
    
    // Core components
    std::unique_ptr<AppConfig> m_config;
    std::unique_ptr<UIManager> m_uiManager;
    std::unique_ptr<SystemTray> m_systemTray;
    
    // Initialize components
    bool InitializeConfig();
    bool InitializeUI();
    bool InitializeSystemTray();
    bool InitializeHotkeys();
    
    // Hotkey handling
    void HandleGlobalHotkey();
    void HandlePomodoroHotkey();
    void HandleQuickCaptureHotkey();
    void HandleShowTodayTasksHotkey();

    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    
    // Settings change callbacks
    void OnSettingsChanged();
    
    // Hotkey parsing
    bool ParseHotkeyString(const std::string& hotkeyStr, UINT& modifiers, UINT& vk);
    
    // Window state management
    void SaveWindowState();
    void RestoreWindowState();

    // Hotkey IDs for settings
    enum HotkeyIDs {
        HOTKEY_GLOBAL_TOGGLE = 1,
        HOTKEY_POMODORO_START = 2,
        HOTKEY_QUICK_CAPTURE = 3,
        HOTKEY_SHOW_TODAY_TASKS = 4
    };
};