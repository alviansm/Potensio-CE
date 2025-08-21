// SystemTray.cpp - FIXED VERSION
#include "SystemTray.h"
#include "app/Application.h"  // Fixed include path
#include "core/Logger.h"
#include <shellapi.h>

// Include resource header with proper guard
#ifdef _WIN32
    #include "../../resources/resource.h"
#endif

const char* SystemTray::WINDOW_CLASS_NAME = "PotensioTrayWindow";

SystemTray::SystemTray()
{
    ZeroMemory(&m_nid, sizeof(m_nid));
}

SystemTray::~SystemTray()
{
    Hide();
    
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        UnregisterClassA(WINDOW_CLASS_NAME, m_hInstance);
    }
}

bool SystemTray::Initialize(HINSTANCE hInstance)
{
    m_hInstance = hInstance;
    
    // Register window class for hidden window
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    
    if (!RegisterClassExA(&wc))
    {
        Logger::Error("Failed to register tray window class");
        return false;
    }
    
    // Create hidden window for tray messages
    m_hwnd = CreateWindowExA(
        0,
        WINDOW_CLASS_NAME,
        "Potensio Tray",
        0,
        0, 0, 0, 0,
        nullptr,
        nullptr,
        hInstance,
        this
    );
    
    if (!m_hwnd)
    {
        Logger::Error("Failed to create tray window");
        return false;
    }
    
    // Create tray icon
    if (!CreateTrayIcon())
    {
        Logger::Error("Failed to create tray icon");
        return false;
    }
    
    // Show the icon immediately
    Show();
    
    Logger::Info("System tray initialized successfully");
    return true;
}

void SystemTray::Show()
{
    if (!m_isVisible && Shell_NotifyIconA(NIM_ADD, &m_nid))
    {
        m_isVisible = true;
        Logger::Debug("System tray icon shown");
    }
}

void SystemTray::Hide()
{
    if (m_isVisible && Shell_NotifyIconA(NIM_DELETE, &m_nid))
    {
        m_isVisible = false;
        Logger::Debug("System tray icon hidden");
    }
}

void SystemTray::SetTooltip(const std::string& tooltip)
{
    if (tooltip.length() < sizeof(m_nid.szTip))
    {
        strcpy_s(m_nid.szTip, tooltip.c_str());
        
        if (m_isVisible)
        {
            Shell_NotifyIconA(NIM_MODIFY, &m_nid);
        }
    }
}

LRESULT CALLBACK SystemTray::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SystemTray* tray = nullptr;
    
    if (uMsg == WM_CREATE)
    {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        tray = static_cast<SystemTray*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(tray));
        DragAcceptFiles(hwnd, TRUE);
    }
    else
    {
        tray = reinterpret_cast<SystemTray*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    if (tray)
    {
        switch (uMsg)
        {
            case WM_TRAYICON:
                switch (lParam)
                {
                      case WM_DROPFILES: {
                        HDROP hDrop = reinterpret_cast<HDROP>(wParam);
                        char filePath[MAX_PATH];
                        UINT fileCount = DragQueryFileA(hDrop, 0xFFFFFFFF, nullptr, 0);

                        for (UINT i = 0; i < fileCount; i++) {
                          DragQueryFileA(hDrop, i, filePath, MAX_PATH);
                          Logger::Info("Dropped file: {}", filePath);

                          // Wake up and show main window
                          if (Application::GetInstance())
                            Application::GetInstance()->ShowMainWindow();

                          // You can now forward `filePath` to your app logic
                        }

                        DragFinish(hDrop);
                        return 0;
                      }
                    case WM_LBUTTONUP:
                        tray->OnTrayIconClicked();
                        break;
                    case WM_LBUTTONDBLCLK:
                        tray->OnTrayIconDoubleClicked();
                        break;
                    case WM_RBUTTONUP:
                        {
                            POINT pt;
                            GetCursorPos(&pt);
                            tray->ShowContextMenu(pt.x, pt.y);
                        }
                        break;
                }
                return 0;
                
            case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                    case MENU_SHOW:
                        if (Application::GetInstance())
                            Application::GetInstance()->ShowMainWindow();
                        break;
                    case MENU_SETTINGS:
                        Logger::Info("Settings menu clicked");
                        break;
                    case MENU_ABOUT:
                        MessageBoxA(nullptr, 
                            "Potensio v0.1.0 - Sprint 1\nproductivity suite\n\nBuilt with Dear ImGui and C++\n\nFeatures: Dropover, File Management, System Tray", 
                            "About Potensio", 
                            MB_ICONINFORMATION);
                        break;
                    case MENU_EXIT:
                            if (Application::GetInstance())
                                Application::GetInstance()->RequestExit();
                            break;
                }
                return 0;
        }
    }
    
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

bool SystemTray::CreateTrayIcon()
{
    m_nid.cbSize = sizeof(NOTIFYICONDATAA);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = TRAY_ICON_ID;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = LoadTrayIcon();
    strcpy_s(m_nid.szTip, "Potensio - Productivity Suite");
    
    return true;
}

bool SystemTray::RemoveTrayIcon()
{
    if (m_isVisible)
    {
        return Shell_NotifyIconA(NIM_DELETE, &m_nid) != FALSE;
    }
    return true;
}

HICON SystemTray::LoadTrayIcon()
{
    HICON icon = nullptr;
    
    // Try to load app icon from resources first
#ifdef _WIN32
    icon = LoadIconA(m_hInstance, MAKEINTRESOURCEA(IDI_MAIN_ICON));
#endif
    
    if (!icon)
    {
        // Try to load custom icon file
        icon = (HICON)LoadImageA(
            m_hInstance,
            "resources/icons/tray_icon.ico",
            IMAGE_ICON,
            16, 16,
            LR_LOADFROMFILE
        );
    }
    
    if (!icon)
    {
        // Fallback to system icon
        icon = LoadIconA(nullptr, IDI_APPLICATION);
        Logger::Debug("Using default system icon for tray");
    }
    else
    {
        Logger::Debug("Loaded custom app icon for tray");
    }
    
    return icon;
}

void SystemTray::ShowContextMenu(int x, int y)
{
    HMENU hMenu = CreatePopupMenu();
    
    AppendMenuA(hMenu, MF_STRING, MENU_SHOW, "Show Potensio");
    AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hMenu, MF_STRING, MENU_SETTINGS, "Settings");
    AppendMenuA(hMenu, MF_STRING, MENU_ABOUT, "About");
    AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hMenu, MF_STRING, MENU_EXIT, "Exit");
    
    // Make the window foreground so the menu appears properly
    SetForegroundWindow(m_hwnd);
    
    // Show context menu
    TrackPopupMenu(
        hMenu,
        TPM_RIGHTBUTTON,
        x, y,
        0,
        m_hwnd,
        nullptr
    );
    
    // Clean up
    DestroyMenu(hMenu);
}

void SystemTray::OnTrayIconClicked()
{
    Logger::Debug("Tray icon clicked");
    
    if (Application::GetInstance())
    {
        Application::GetInstance()->ToggleMainWindow();
    }
}

void SystemTray::OnTrayIconDoubleClicked()
{
    Logger::Debug("Tray icon double-clicked");
    
    if (Application::GetInstance())
    {
        Application::GetInstance()->ShowMainWindow();
    }
}