#pragma once

#include <windows.h>
#include <memory>
#include <string>

// Forward declarations
class MainWindow;
class SettingsWindow;
class AppConfig;

class UIManager
{
public:
    struct WindowInfo
    {
        int x = 100;
        int y = 100;
        int width = 1000;
        int height = 700;
        bool maximized = false;
    };

public:
    UIManager();
    ~UIManager();

    // Initialization
    bool Initialize(HINSTANCE hInstance);
    void Shutdown();

    // Main loop functions
    void NewFrame();
    void Update(float deltaTime);
    void Render();

    // Window management
    void ShowWindow();
    void HideWindow();
    bool IsWindowVisible() const;

    // Window state
    void SetWindowInfo(const WindowInfo& info);
    WindowInfo GetWindowInfo() const;

    // Component access
    MainWindow* GetMainWindow() { return m_mainWindow.get(); }

private:
    // Window creation and setup
    bool CreateMainWindow(HINSTANCE hInstance);
    bool InitializeImGui();
    bool InitializeOpenGL();
    void SetupImGuiStyle();

    // Event handling
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    // Window handles
    HWND m_hwnd;
    HDC m_hdc;
    HGLRC m_hglrc;
    
    // Window state
    bool m_isVisible;
    bool m_isInitialized;
    WindowInfo m_windowInfo;
    
    // UI Components
    std::unique_ptr<MainWindow> m_mainWindow;
    std::unique_ptr<SettingsWindow> m_settingsWindow;
    
    // Configuration
    AppConfig* m_config;
    
    // Window class name
    static constexpr const char* WINDOW_CLASS_NAME = "PotensioMainWindow";
};