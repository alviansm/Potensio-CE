#pragma once

#include <windows.h>
#include <memory>
#include <string>

#define WM_SHOW_POTENSIO (WM_USER + 1)

// Forward declarations
class UIManager;
class SystemTray;
class AppConfig;

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

private:
    // Static instance for singleton access
    static Application* s_instance;
    
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
    
    // Window state management
    void SaveWindowState();
    void RestoreWindowState();
};