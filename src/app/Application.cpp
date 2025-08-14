// Updated Application.cpp with proper hotkey message handling
#include "Application.h"
#include "AppConfig.h"
#include "SystemTray.h"
#include "ui/UIManager.h"
#include "core/Logger.h"
#include "platform/windows/WindowsHooks.h"
#include <chrono>

// Static instance
Application* Application::s_instance = nullptr;

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
        // Initialize hooks system
        if (!WindowsHooks::Initialize())
        {
            Logger::Warning("Failed to initialize Windows hooks system");
            return false;
        }
        
        // Register default hotkey: Ctrl+Shift+P using callback method
        UINT modifiers = MOD_CONTROL | MOD_SHIFT;
        UINT vk = 'P';
        
        // Use callback method instead of hook proc
        auto hotkeyCallback = []() {
            if (s_instance)
            {
                s_instance->HandleGlobalHotkey();
            }
        };
        
        if (!WindowsHooks::RegisterHotkeyCallback(1, modifiers, vk, hotkeyCallback))
        {
            Logger::Warning("Failed to register global hotkey Ctrl+Shift+P");
            // Don't fail initialization for hotkey registration failure
        }
        else
        {
            Logger::Info("Global hotkey Ctrl+Shift+P registered successfully");
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