// Updated Application.cpp with proper hotkey message handling
#include "Application.h"
#include "AppConfig.h"
#include "SystemTray.h"
#include "ui/UIManager.h"
#include "core/Logger.h"
#include "platform/windows/WindowsHooks.h"
#include "app/MouseTracker.h"
#include <chrono>

// Static instance
Application* Application::s_instance = nullptr;
HHOOK Application::g_mouseHook = nullptr;

Application::Application()
{
    s_instance = this;
}

Application::~Application()
{
    Shutdown();
    s_instance = nullptr;
}

bool Application::Initialize(HINSTANCE hInstance)
{
    m_hInstance = hInstance;
    
    Logger::Info("Initializing Potensio Application...");
    
    // Initialize components in order
    if (!InitializeConfig())
    {
        Logger::Error("Failed to initialize configuration");
        return false;
    }
    
    if (!InitializeUI())
    {
        Logger::Error("Failed to initialize UI");
        return false;
    }
    
    if (!InitializeSystemTray())
    {
        Logger::Error("Failed to initialize system tray");
        return false;
    }
    
    if (!InitializeHotkeys())
    {
        Logger::Error("Failed to initialize hotkeys");
        return false;
    }

    g_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, nullptr, 0);
    
    // Apply settings-based configurations
    ApplyStartupSettings();
    RegisterSettingsHotkeys();
    
    // Restore window state
    RestoreWindowState();
    
    Logger::Info("Application initialized successfully");
    return true;
}

int Application::Run()
{
    Logger::Info("Starting main application loop");
    
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    
    while (!m_shouldExit)
    {
        // Calculate frame time for smooth animations
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
        lastFrameTime = currentTime;
        
        // Process Windows messages
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            // Check if WindowsHooks wants to handle this message (e.g., hotkeys)
            if (!WindowsHooks::ProcessMessage(&msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            
            if (msg.message == WM_QUIT)
            {
                m_shouldExit = true;
                break;
            }
        }
        
        if (m_shouldExit)
            break;
            
        // Update UI if window is visible
        if (m_isVisible && m_uiManager)
        {
            m_uiManager->NewFrame();
            m_uiManager->Update(deltaTime);
            m_uiManager->Render();
        }
        
        // Small sleep to prevent 100% CPU usage
        Sleep(1);
    }
    
    Logger::Info("Main loop exited");
    return 0;
}

void Application::Shutdown()
{
    Logger::Info("Shutting down application");
    
    // Save current state
    SaveWindowState();
    
    // Unregister hotkeys
    UnregisterSettingsHotkeys();

    if (g_mouseHook)
      UnhookWindowsHookEx(g_mouseHook);
    
    // Cleanup in reverse order
    WindowsHooks::Cleanup();
    m_systemTray.reset();
    m_uiManager.reset();
    m_config.reset();
}

void Application::ShowMainWindow()
{
    if (m_uiManager)
    {
        m_uiManager->ShowWindow();
        m_isVisible = true;
        Logger::Debug("Main window shown");
    }
}

void Application::HideMainWindow()
{
    if (m_uiManager)
    {
        m_uiManager->HideWindow();
        m_isVisible = false;
        Logger::Debug("Main window hidden");
    }
}

void Application::ToggleMainWindow()
{
    if (m_isVisible)
        HideMainWindow();
    else
        ShowMainWindow();
}

bool Application::InitializeConfig()
{
    try
    {
        m_config = std::make_unique<AppConfig>();
        m_config->Load();
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::Error("Config initialization failed: {}", e.what());
        return false;
    }
}

bool Application::InitializeUI()
{
    try
    {
        m_uiManager = std::make_unique<UIManager>();
        if (!m_uiManager->Initialize(m_hInstance))
        {
            Logger::Error("UI Manager initialization failed");
            return false;
        }
        
        // Start hidden by default
        m_isVisible = false;
        m_uiManager->HideWindow();
        
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::Error("UI initialization failed: {}", e.what());
        return false;
    }
}

bool Application::InitializeSystemTray()
{
    try
    {
        m_systemTray = std::make_unique<SystemTray>();
        return m_systemTray->Initialize(m_hInstance);
    }
    catch (const std::exception& e)
    {
        Logger::Error("System tray initialization failed: {}", e.what());
        return false;
    }
}

bool Application::InitializeHotkeys()
{
    try
    {
        if (!WindowsHooks::Initialize())
        {
            Logger::Warning("Failed to initialize Windows hooks system");
            return false;
        }

        // Only register if enabled
        if (m_hotkeysEnabled)
        {
            UINT modifiers = MOD_CONTROL | MOD_SHIFT;
            UINT vk = 'Q';

            auto hotkeyCallback = []() {
                if (s_instance)
                    s_instance->HandleGlobalHotkey();
            };

            if (!WindowsHooks::RegisterHotkeyCallback(1, modifiers, vk, hotkeyCallback))
            {
                Logger::Warning("Failed to register global hotkey Ctrl+Shift+Q");
            }
            else
            {
                Logger::Info("Global hotkey Ctrl+Shift+Q registered successfully");
            }
        }

        return true;
    }
    catch (const std::exception& e)
    {
        Logger::Error("Hotkey initialization failed: {}", e.what());
        return false;
    }
}


void Application::HandleGlobalHotkey()
{
    Logger::Debug("Global hotkey triggered");
    ToggleMainWindow();
}

void Application::SaveWindowState()
{
    if (m_config && m_uiManager)
    {
        auto windowInfo = m_uiManager->GetWindowInfo();
        
        m_config->SetValue("window.x", windowInfo.x);
        m_config->SetValue("window.y", windowInfo.y);
        m_config->SetValue("window.width", windowInfo.width);
        m_config->SetValue("window.height", windowInfo.height);
        m_config->SetValue("window.maximized", windowInfo.maximized);
        
        m_config->Save();
        Logger::Debug("Window state saved");
    }
}

void Application::RestoreWindowState()
{
    if (m_config && m_uiManager)
    {
        UIManager::WindowInfo windowInfo;
        windowInfo.x = m_config->GetValue("window.x", 100);
        windowInfo.y = m_config->GetValue("window.y", 100);
        windowInfo.width = m_config->GetValue("window.width", 1000);
        windowInfo.height = m_config->GetValue("window.height", 700);
        windowInfo.maximized = m_config->GetValue("window.maximized", false);
        
        m_uiManager->SetWindowInfo(windowInfo);
        Logger::Debug("Window state restored");
    }
}

void Application::ApplyStartupSettings()
{
    if (!m_config) return;
    
    // Apply startup with Windows setting
    bool startWithWindows = m_config->GetValue("general.start_with_windows", false);
    RegisterStartupWithWindows(startWithWindows);
    
    // Apply startup minimized setting
    bool startMinimized = m_config->GetValue("general.start_minimized", false);
    if (startMinimized)
    {
        m_isVisible = false;
        if (m_uiManager)
        {
            m_uiManager->HideWindow();
        }
    }
    
    Logger::Info("Startup settings applied");
}

void Application::RegisterStartupWithWindows(bool enable)
{
    const char* APP_NAME = "Potensio";
    const char* REG_KEY = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
    
    HKEY hKey;
    LONG result = RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_SET_VALUE, &hKey);
    
    if (result == ERROR_SUCCESS)
    {
        if (enable)
        {
            // Get the current executable path
            char exePath[MAX_PATH];
            GetModuleFileNameA(nullptr, exePath, MAX_PATH);
            
            // Add quotes around path to handle spaces
            std::string quotedPath = "\"" + std::string(exePath) + "\" --startup";
            
            // Set the registry value
            result = RegSetValueExA(hKey, APP_NAME, 0, REG_SZ, 
                                   (BYTE*)quotedPath.c_str(), 
                                   static_cast<DWORD>(quotedPath.length() + 1));
            
            if (result == ERROR_SUCCESS)
            {
                Logger::Info("Startup with Windows enabled");
            }
            else
            {
                Logger::Error("Failed to enable startup with Windows: {}", result);
            }
        }
        else
        {
            // Remove the registry value
            result = RegDeleteValueA(hKey, APP_NAME);
            
            if (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
            {
                Logger::Info("Startup with Windows disabled");
            }
            else
            {
                Logger::Error("Failed to disable startup with Windows: {}", result);
            }
        }
        
        RegCloseKey(hKey);
    }
    else
    {
        Logger::Error("Failed to open registry key for startup settings: {}", result);
    }
}

void Application::RegisterSettingsHotkeys()
{
    if (!m_config) return;
    
    // Unregister existing hotkeys first
    UnregisterSettingsHotkeys();
    
    std::string defaultGlobal("Ctrl+Shift+Q");
    std::string defaultPomodoro("Ctrl+Alt+P");
    std::string defaultCapture("Ctrl+Shift+C");
    std::string defaultTasks("Ctrl+Shift+T");
    
    std::string globalHotkey = m_config->GetValue("hotkeys.global_hotkey", defaultGlobal);
    std::string pomodoroHotkey = m_config->GetValue("hotkeys.pomodoro_start", defaultPomodoro);
    std::string captureHotkey = m_config->GetValue("hotkeys.quick_capture", defaultCapture);
    std::string tasksHotkey = m_config->GetValue("hotkeys.show_today_tasks", defaultTasks);
    
    // Parse and register global toggle hotkey
    UINT modifiers, vk;
    if (ParseHotkeyString(globalHotkey, modifiers, vk))
    {
        auto globalCallback = []() {
            if (s_instance) {
                s_instance->ToggleMainWindow();
            }
        };
        
        if (WindowsHooks::RegisterHotkeyCallback(HOTKEY_GLOBAL_TOGGLE, modifiers, vk, globalCallback))
        {
            Logger::Info("Global hotkey registered: {}", globalHotkey);
        }
    }
    
    // Parse and register Pomodoro hotkey
    if (ParseHotkeyString(pomodoroHotkey, modifiers, vk))
    {
        auto pomodoroCallback = []() {
            if (s_instance) {
                s_instance->HandlePomodoroHotkey();
            }
        };
        
        if (WindowsHooks::RegisterHotkeyCallback(HOTKEY_POMODORO_START, modifiers, vk, pomodoroCallback))
        {
            Logger::Info("Pomodoro hotkey registered: {}", pomodoroHotkey);
        }
    }
    
    // Parse and register quick capture hotkey
    if (ParseHotkeyString(captureHotkey, modifiers, vk))
    {
        auto captureCallback = []() {
            if (s_instance) {
                s_instance->HandleQuickCaptureHotkey();
            }
        };
        
        if (WindowsHooks::RegisterHotkeyCallback(HOTKEY_QUICK_CAPTURE, modifiers, vk, captureCallback))
        {
            Logger::Info("Quick capture hotkey registered: {}", captureHotkey);
        }
    }
    
    // Parse and register today tasks hotkey
    if (ParseHotkeyString(tasksHotkey, modifiers, vk))
    {
        auto tasksCallback = []() {
            if (s_instance) {
                s_instance->HandleShowTodayTasksHotkey();
            }
        };
        
        if (WindowsHooks::RegisterHotkeyCallback(HOTKEY_SHOW_TODAY_TASKS, modifiers, vk, tasksCallback))
        {
            Logger::Info("Today tasks hotkey registered: {}", tasksHotkey);
        }
    }
}

void Application::UnregisterSettingsHotkeys()
{
    WindowsHooks::UnregisterHotkey(HOTKEY_GLOBAL_TOGGLE);
    WindowsHooks::UnregisterHotkey(HOTKEY_POMODORO_START);
    WindowsHooks::UnregisterHotkey(HOTKEY_QUICK_CAPTURE);
    WindowsHooks::UnregisterHotkey(HOTKEY_SHOW_TODAY_TASKS);
}

bool Application::ParseHotkeyString(const std::string& hotkeyStr, UINT& modifiers, UINT& vk)
{
    if (hotkeyStr.empty()) return false;
    
    modifiers = 0;
    vk = 0;
    
    // Simple parser for hotkey strings like "Ctrl+Shift+Q"
    std::string str = hotkeyStr;
    
    // Check for modifiers
    if (str.find("Ctrl+") != std::string::npos)
    {
        modifiers |= MOD_CONTROL;
        str.erase(str.find("Ctrl+"), 5);
    }
    
    if (str.find("Alt+") != std::string::npos)
    {
        modifiers |= MOD_ALT;
        str.erase(str.find("Alt+"), 4);
    }
    
    if (str.find("Shift+") != std::string::npos)
    {
        modifiers |= MOD_SHIFT;
        str.erase(str.find("Shift+"), 6);
    }
    
    if (str.find("Win+") != std::string::npos)
    {
        modifiers |= MOD_WIN;
        str.erase(str.find("Win+"), 4);
    }
    
    // Get the main key
    if (str.length() == 1)
    {
        // Single character key
        char c = str[0];
        if (c >= 'A' && c <= 'Z')
        {
            vk = c;
        }
        else if (c >= 'a' && c <= 'z')
        {
            vk = c - 'a' + 'A'; // Convert to uppercase
        }
        else if (c >= '0' && c <= '9')
        {
            vk = c;
        }
    }
    else
    {
        // Function keys and special keys
        if (str.substr(0, 1) == "F" && str.length() <= 3)
        {
            // Function key (F1-F12)
            int fnum = std::atoi(str.substr(1).c_str());
            if (fnum >= 1 && fnum <= 12)
            {
                vk = VK_F1 + fnum - 1;
            }
        }
        else
        {
            // Special keys
            if (str == "Space") vk = VK_SPACE;
            else if (str == "Enter") vk = VK_RETURN;
            else if (str == "Tab") vk = VK_TAB;
            else if (str == "Esc") vk = VK_ESCAPE;
            else if (str == "Delete") vk = VK_DELETE;
            else if (str == "Insert") vk = VK_INSERT;
            else if (str == "Home") vk = VK_HOME;
            else if (str == "End") vk = VK_END;
            else if (str == "PageUp") vk = VK_PRIOR;
            else if (str == "PageDown") vk = VK_NEXT;
            // Add more special keys as needed
        }
    }
    
    return (vk != 0);
}

// Hotkey handlers
void Application::HandlePomodoroHotkey()
{
    Logger::Debug("Pomodoro hotkey triggered");
    
    // Show window if hidden
    if (!m_isVisible)
    {
        ShowMainWindow();
    }
    
    // Switch to Pomodoro module and start/stop timer
    // TODO: Implement direct access to MainWindow to switch modules
    // This would require adding a method to MainWindow to switch modules
    // and access the Pomodoro timer
}

void Application::HandleQuickCaptureHotkey()
{
    Logger::Debug("Quick capture hotkey triggered");
    
    // TODO: Implement quick capture functionality
    // This could capture clipboard content, take a screenshot, etc.
}

void Application::HandleShowTodayTasksHotkey()
{
    Logger::Debug("Show today tasks hotkey triggered");
    
    // Show window if hidden
    if (!m_isVisible)
    {
        ShowMainWindow();
    }
    
    // TODO: Switch to Todo module and show today's tasks
}

LRESULT CALLBACK Application::LowLevelMouseProc(int nCode, WPARAM wParam,
                                                LPARAM lParam) {
  if (nCode == HC_ACTION) {
    auto *p = reinterpret_cast<MSLLHOOKSTRUCT *>(lParam);

    switch (wParam) {
    case WM_LBUTTONDOWN:
      MouseTracker::Get().OnDragStart(p->pt);
      break;

    case WM_MOUSEMOVE:
      if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
        MouseTracker::Get().OnMouseMove(p->pt);
      break;

    case WM_LBUTTONUP:
      MouseTracker::Get().OnDragEnd();
      break;
    }
  }

  return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}


// Settings change notification
void Application::OnSettingsChanged()
{
    Logger::Info("Settings changed, applying updates...");
    
    // Reapply startup settings
    ApplyStartupSettings();
    
    // Update hotkeys
    RegisterSettingsHotkeys();
    
    // Apply other system-level settings
    ApplySystemSettings();
}

void Application::ApplySystemSettings()
{
    if (!m_config) return;
    
    // Apply notification settings
    bool showNotifications = m_config->GetValue("general.show_notifications", true);
    // TODO: Configure system notification settings
    
    // Apply sound settings
    bool enableSounds = m_config->GetValue("general.enable_sounds", true);
    // TODO: Configure sound system
    
    // Apply tray behavior
    bool minimizeToTray = m_config->GetValue("general.minimize_to_tray", true);
    bool closeToTray = m_config->GetValue("general.close_to_tray", true);
    
    // Note: Removed SystemTray method calls that don't exist
    // These settings can be used elsewhere or implemented later
    Logger::Info("Tray behavior - Minimize to tray: {}, Close to tray: {}", 
                minimizeToTray, closeToTray);
    
    Logger::Info("System settings applied");
}

void Application::UpdateHotkeys()
{
    // Alias for RegisterSettingsHotkeys
    RegisterSettingsHotkeys();
}

void Application::UpdateSettingsFromConfig()
{
    OnSettingsChanged();
}

void Application::EnableHotkeys(bool enable)
{
    if (m_hotkeysEnabled == enable)
        return; // nothing to do

    m_hotkeysEnabled = enable;

    UINT modifiers = MOD_CONTROL | MOD_SHIFT;
    UINT vk = 'Q';

    if (enable)
    {
        auto hotkeyCallback = []() {
            if (s_instance)
                s_instance->HandleGlobalHotkey();
        };

        if (WindowsHooks::RegisterHotkeyCallback(1, modifiers, vk, hotkeyCallback))
            Logger::Info("Global hotkey Ctrl+Shift+Q enabled");
        else
            Logger::Warning("Failed to re-enable hotkey");
    }
    else
    {
        WindowsHooks::UnregisterHotkey(1); // assumes you have this implemented
        Logger::Info("Global hotkey Ctrl+Shift+Q disabled");
    }
}
