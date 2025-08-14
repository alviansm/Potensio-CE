#pragma once

#include <windows.h>
#include <shellapi.h>
#include <string>

class SystemTray
{
public:
    SystemTray();
    ~SystemTray();

    // Initialize system tray
    bool Initialize(HINSTANCE hInstance);
    
    // Show/hide tray icon
    void Show();
    void Hide();
    
    // Update tray icon tooltip
    void SetTooltip(const std::string& tooltip);
    
    // Check if tray is visible
    bool IsVisible() const { return m_isVisible; }

private:
    static const UINT WM_TRAYICON = WM_USER + 1;
    static const UINT TRAY_ICON_ID = 1;
    
    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    NOTIFYICONDATAA m_nid = {};
    bool m_isVisible = false;
    
    // Window class and message handling
    static const char* WINDOW_CLASS_NAME;
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // Tray icon management
    bool CreateTrayIcon();
    bool RemoveTrayIcon();
    HICON LoadTrayIcon();
    
    // Context menu
    void ShowContextMenu(int x, int y);
    void OnTrayIconClicked();
    void OnTrayIconDoubleClicked();
    
    // Menu item IDs
    enum MenuItems
    {
        MENU_SHOW = 1000,
        MENU_SETTINGS,
        MENU_ABOUT,
        MENU_EXIT
    };
};