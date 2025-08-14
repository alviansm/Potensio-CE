#pragma once

#include <windows.h>
#include <memory>

// Forward declarations
class MainWindow;
class SettingsWindow;

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

    UIManager();
    ~UIManager();

    // Initialize UI system
    bool Initialize(HINSTANCE hInstance);
    
    // Shutdown UI system
    void Shutdown();
    
    // Main UI loop functions
    void NewFrame();
    void Update(float deltaTime);
    void Render();
    
    // Window management
    void ShowWindow();
    void HideWindow();
    bool IsWindowVisible() const;
    
    // Window state
    WindowInfo GetWindowInfo() const;
    void SetWindowInfo(const WindowInfo& info);
    
    // Get window handle
    HWND GetWindowHandle() const { return m_hwnd; }

private:
    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    HDC m_hdc = nullptr;
    HGLRC m_hglrc = nullptr;
    
    bool m_isInitialized = false;
    bool m_isVisible = false;
    
    // UI Windows
    std::unique_ptr<MainWindow> m_mainWindow;
    std::unique_ptr<SettingsWindow> m_settingsWindow;
    
    // Window management
    static const char* WINDOW_CLASS_NAME;
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // OpenGL and ImGui setup
    bool InitializeOpenGL();
    bool InitializeImGui();
    void CleanupOpenGL();
    void CleanupImGui();
    
    // Window creation
    bool CreateMainWindow();
    
    // Event handling
    void OnWindowClose();
    void OnWindowResize(int width, int height);
};