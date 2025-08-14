// UIManager.cpp - FIXED IMGUI VERSION
#include "UIManager.h"
#include "Windows/MainWindow.h"
#include "Windows/SettingsWindow.h"
#include "app/Application.h"
#include "core/Logger.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_opengl3.h>
#include <GL/gl.h>

// Include resource header with proper guard
#ifdef _WIN32
    #include "../../resources/resource.h"
#endif

const char* UIManager::WINDOW_CLASS_NAME = "PotensioMainWindow";

UIManager::UIManager()
{
}

UIManager::~UIManager()
{
    Shutdown();
}

bool UIManager::Initialize(HINSTANCE hInstance)
{
    m_hInstance = hInstance;
    
    Logger::Info("Initializing UI Manager...");
    
    // Create main window
    if (!CreateMainWindow())
    {
        Logger::Error("Failed to create main window");
        return false;
    }
    
    // Initialize OpenGL
    if (!InitializeOpenGL())
    {
        Logger::Error("Failed to initialize OpenGL");
        return false;
    }
    
    // Initialize ImGui
    if (!InitializeImGui())
    {
        Logger::Error("Failed to initialize ImGui");
        return false;
    }
    
    // Create UI windows
    m_mainWindow = std::make_unique<MainWindow>();
    m_settingsWindow = std::make_unique<SettingsWindow>();

    // oad textures now that OpenGL context exists
    m_mainWindow->InitializeResources();
    
    m_isInitialized = true;
    Logger::Info("UI Manager initialized successfully");
    
    return true;
}

void UIManager::Shutdown()
{
    if (!m_isInitialized)
        return;
        
    Logger::Info("Shutting down UI Manager");
    
    // Cleanup UI windows
    m_settingsWindow.reset();
    m_mainWindow.reset();
    
    // Cleanup ImGui and OpenGL
    CleanupImGui();
    CleanupOpenGL();
    
    // Destroy window
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    
    // Unregister window class
    UnregisterClassA(WINDOW_CLASS_NAME, m_hInstance);
    
    m_isInitialized = false;
}

void UIManager::NewFrame()
{
    if (!m_isInitialized || !m_isVisible)
        return;
        
    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void UIManager::Update(float deltaTime)
{
    if (!m_isInitialized || !m_isVisible)
        return;
        
    // Update main window
    if (m_mainWindow)
    {
        m_mainWindow->Update(deltaTime);
    }
    
    // Update settings window if visible
    if (m_settingsWindow && m_settingsWindow->IsVisible())
    {
        m_settingsWindow->Update(deltaTime);
    }
}

void UIManager::Render()
{
    if (!m_isInitialized || !m_isVisible)
        return;
        
    // Clear background
    glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Render main window
    if (m_mainWindow)
    {
        m_mainWindow->Render();
    }
    
    // Render settings window if visible
    if (m_settingsWindow && m_settingsWindow->IsVisible())
    {
        m_settingsWindow->Render();
    }
    
    // Render ImGui
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // Present
    SwapBuffers(m_hdc);
}

void UIManager::ShowWindow()
{
    if (m_hwnd)
    {
        ::ShowWindow(m_hwnd, SW_SHOW);
        SetForegroundWindow(m_hwnd);
        m_isVisible = true;
    }
}

void UIManager::HideWindow()
{
    if (m_hwnd)
    {
        ::ShowWindow(m_hwnd, SW_HIDE);
        m_isVisible = false;
    }
}

bool UIManager::IsWindowVisible() const
{
    return m_isVisible && ::IsWindowVisible(m_hwnd);
}

UIManager::WindowInfo UIManager::GetWindowInfo() const
{
    WindowInfo info;
    
    if (m_hwnd)
    {
        RECT rect;
        GetWindowRect(m_hwnd, &rect);
        
        info.x = rect.left;
        info.y = rect.top;
        info.width = rect.right - rect.left;
        info.height = rect.bottom - rect.top;
        info.maximized = IsZoomed(m_hwnd) != FALSE;
    }
    
    return info;
}

void UIManager::SetWindowInfo(const WindowInfo& info)
{
    if (m_hwnd)
    {
        if (info.maximized)
        {
            ::ShowWindow(m_hwnd, SW_MAXIMIZE);
        }
        else
        {
            SetWindowPos(m_hwnd, nullptr, info.x, info.y, info.width, info.height, 
                        SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
}

LRESULT CALLBACK UIManager::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // Handle ImGui messages - use extern declaration for better compatibility
    extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
        return true;
        
    UIManager* ui = nullptr;
    
    if (uMsg == WM_CREATE)
    {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        ui = static_cast<UIManager*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ui));
    }
    else
    {
        ui = reinterpret_cast<UIManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    switch (uMsg)
    {
        case WM_CLOSE:
            if (ui) ui->OnWindowClose();
            return 0;
            
        case WM_SIZE:
            if (ui && wParam != SIZE_MINIMIZED)
            {
                ui->OnWindowResize(LOWORD(lParam), HIWORD(lParam));
            }
            return 0;

        case WM_SHOW_POTENSIO:
            if (Application::GetInstance())
            {
                Application::GetInstance()->ShowMainWindow();
            }
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

bool UIManager::CreateMainWindow()
{
    // Load app icon
    HICON hIcon = nullptr;
    
#ifdef _WIN32
    hIcon = LoadIconA(m_hInstance, MAKEINTRESOURCEA(IDI_MAIN_ICON));
#endif
    
    if (!hIcon)
    {
        hIcon = LoadIconA(nullptr, IDI_APPLICATION); // Fallback
        Logger::Debug("Using default application icon");
    }
    else
    {
        Logger::Info("Loaded custom application icon");
    }
    
    // Register window class
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.hIcon = hIcon;                    // Large icon
    wc.hIconSm = hIcon;                  // Small icon
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    
    if (!RegisterClassExA(&wc))
    {
        Logger::Error("Failed to register main window class");
        return false;
    }
    
    // Create smaller default window
    m_hwnd = CreateWindowExA(
        0,
        WINDOW_CLASS_NAME,
        "Potensio - Productivity Suite",
        WS_OVERLAPPEDWINDOW,
        200, 200, 550, 400,  // Smaller default size
        nullptr,
        nullptr,
        m_hInstance,
        this
    );
    
    if (!m_hwnd)
    {
        Logger::Error("Failed to create main window");
        return false;
    }
    
    // Set window icon explicitly
    SendMessage(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    
    // Get device context
    m_hdc = GetDC(m_hwnd);
    if (!m_hdc)
    {
        Logger::Error("Failed to get device context");
        return false;
    }
    
    return true;
}

bool UIManager::InitializeOpenGL()
{
    // Set pixel format
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    
    int pixelFormat = ChoosePixelFormat(m_hdc, &pfd);
    if (!pixelFormat)
    {
        Logger::Error("Failed to choose pixel format");
        return false;
    }
    
    if (!SetPixelFormat(m_hdc, pixelFormat, &pfd))
    {
        Logger::Error("Failed to set pixel format");
        return false;
    }
    
    // Create OpenGL context
    m_hglrc = wglCreateContext(m_hdc);
    if (!m_hglrc)
    {
        Logger::Error("Failed to create OpenGL context");
        return false;
    }
    
    if (!wglMakeCurrent(m_hdc, m_hglrc))
    {
        Logger::Error("Failed to make OpenGL context current");
        return false;
    }
    
    Logger::Info("OpenGL initialized successfully");
    return true;
}

bool UIManager::InitializeImGui()
{
    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Enable features
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Setup style
    ImGui::StyleColorsDark();
    
    // Customize for better UI
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(4, 3);
    style.ItemSpacing = ImVec2(4, 4);
    style.ItemInnerSpacing = ImVec2(4, 4);
    style.ScrollbarSize = 14;
    style.GrabMinSize = 10;
    
    // Setup fonts
    // io.Fonts->AddFontDefault();

    // ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f);
    // if (font)
    // {
    //     io.FontDefault = font;

    //     // Merge emoji font
    //     ImFontConfig config;
    //     config.MergeMode = true;
    //     config.PixelSnapH = true;

    //     // Include a larger emoji range (icons, faces, transport, symbols)
    //     static const ImWchar emoji_ranges[] = {
    //         0x1F300, 0x1F5FF, // Misc Symbols and Pictographs
    //         0x1F600, 0x1F64F, // Emoticons
    //         0x1F680, 0x1F6FF, // Transport and Map
    //         0x2600,  0x26FF,  // Misc symbols
    //         0x2700,  0x27BF,  // Dingbats
    //         0
    //     };

    //     io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguiemj.ttf", 16.0f, &config, emoji_ranges);

    //     io.Fonts->Build();
    // }
    // else
    // {
    //     io.FontGlobalScale = 1.2f;
    // }
    
    // Platform/Renderer backends - must come after fonts are loaded
    if (!ImGui_ImplWin32_Init(m_hwnd))
    {
        Logger::Error("Failed to initialize ImGui Win32 backend");
        return false;
    }
    
    if (!ImGui_ImplOpenGL3_Init("#version 130"))
    {
        Logger::Error("Failed to initialize ImGui OpenGL3 backend");
        return false;
    }

    ImGui::StyleColorsDark();
    
    Logger::Info("ImGui initialized successfully");
    return true;
}


// bool UIManager::InitializeImGui()
// {
//     // Setup ImGui context
//     IMGUI_CHECKVERSION();
//     ImGui::CreateContext();
//     ImGuiIO& io = ImGui::GetIO();
    
//     // Enable features
//     io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
//     // Setup style
//     ImGui::StyleColorsDark();
    
//     // Customize for better UI
//     ImGuiStyle& style = ImGui::GetStyle();
//     style.WindowPadding = ImVec2(8, 8);
//     style.FramePadding = ImVec2(4, 3);
//     style.ItemSpacing = ImVec2(4, 4);
//     style.ItemInnerSpacing = ImVec2(4, 4);
//     style.ScrollbarSize = 14;
//     style.GrabMinSize = 10;
    
//     // Setup fonts - match title bar font size (around 16px)
//     io.Fonts->AddFontDefault();
//     ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f);
//     if (font)
//     {
//         io.FontDefault = font;
//         Logger::Info("Loaded Segoe UI font at 16px");
//     }
//     else
//     {
//         // Fallback to default font with larger scale
//         io.FontGlobalScale = 1.2f;
//         Logger::Info("Using default font with 1.2x scale");
//     }
    
//     // Platform/Renderer backends
//     if (!ImGui_ImplWin32_Init(m_hwnd))
//     {
//         Logger::Error("Failed to initialize ImGui Win32 backend");
//         return false;
//     }
    
//     if (!ImGui_ImplOpenGL3_Init("#version 130"))
//     {
//         Logger::Error("Failed to initialize ImGui OpenGL3 backend");
//         return false;
//     }

//     ImGui::StyleColorsDark();
    
//     Logger::Info("ImGui initialized successfully");
//     return true;
// }

void UIManager::CleanupOpenGL()
{
    if (m_hglrc)
    {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_hglrc);
        m_hglrc = nullptr;
    }
    
    if (m_hdc && m_hwnd)
    {
        ReleaseDC(m_hwnd, m_hdc);
        m_hdc = nullptr;
    }
}

void UIManager::CleanupImGui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void UIManager::OnWindowClose()
{
    Logger::Debug("Main window close requested");
    
    // Hide to tray instead of closing
    HideWindow();
    
    // Don't exit application - just hide window
}

void UIManager::OnWindowResize(int width, int height)
{
    if (m_hglrc)
    {
        glViewport(0, 0, width, height);
    }
}