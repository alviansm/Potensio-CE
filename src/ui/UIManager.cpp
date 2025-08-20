#include "ui/UIManager.h"
#include "ui/Windows/MainWindow.h"
#include "ui/Windows/SettingsWindow.h"
#include "app/Application.h"
#include "app/AppConfig.h"
#include "core/Logger.h"
#include "FileDropTarget.h"
#include "resource.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_opengl3.h>
#include <GL/gl.h>

// Forward declare Win32 message handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

UIManager::UIManager()
    : m_hwnd(nullptr)
    , m_hdc(nullptr)
    , m_hglrc(nullptr)
    , m_isVisible(false)
    , m_isInitialized(false)
    , m_config(nullptr)
{
}

UIManager::~UIManager()
{
    Shutdown();
}

bool UIManager::Initialize(HINSTANCE hInstance)
{
    // Load the icon from resources
    m_hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    m_hIconSmall = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON), 
                                     IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

    // Get configuration
    auto app = Application::GetInstance();
    m_config = app ? app->GetConfig() : nullptr;

    if (!CreateMainWindow(hInstance))
    {
        Logger::Error("Failed to create main window");
        return false;
    }

    if (!InitializeOpenGL())
    {
        Logger::Error("Failed to initialize OpenGL");
        return false;
    }

    if (!InitializeImGui())
    {
        Logger::Error("Failed to initialize ImGui");
        return false;
    }

    // Initialize UI components
    try
    {
        m_mainWindow = std::make_unique<MainWindow>();
        m_settingsWindow = std::make_unique<SettingsWindow>();

        // Initialize MainWindow
        if (!m_mainWindow->Initialize(m_config))
        {
            Logger::Error("Failed to initialize MainWindow");
            return false;
        }

        // SettingsWindow doesn't need explicit initialization
        // It manages its own configuration loading

        // Initialize MainWindow resources (icons, etc.)
        m_mainWindow->InitializeResources();
    }
    catch (const std::exception& e)
    {
        Logger::Error("Exception during UI component initialization: {}", e.what());
        return false;
    }

    SetupImGuiStyle();
    m_isInitialized = true;

    DragAcceptFiles(m_hwnd, TRUE);
    //CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    //RegisterDragDrop(m_hwnd, new FileDropTarget(this));

    Logger::Info("UIManager initialized successfully");
    return true;
}

void UIManager::Shutdown()
{
    if (!m_isInitialized)
        return;

    // Shutdown UI components
    if (m_mainWindow)
        m_mainWindow->Shutdown();

    // SettingsWindow doesn't need explicit shutdown
    // It manages its own cleanup in destructor

    // Reset components
    m_settingsWindow.reset();
    m_mainWindow.reset();

    // Shutdown ImGui
    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    // Cleanup OpenGL
    if (m_hglrc)
    {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_hglrc);
        m_hglrc = nullptr;
    }

    if (m_hdc)
    {
        ReleaseDC(m_hwnd, m_hdc);
        m_hdc = nullptr;
    }

    RevokeDragDrop(m_hwnd);
    CoUninitialize();

    // Destroy window
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    m_isInitialized = false;
    Logger::Debug("UIManager shutdown complete");
}

void UIManager::NewFrame()
{
    if (!m_isInitialized)
        return;

    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void UIManager::Update(float deltaTime)
{
    if (!m_isInitialized)
        return;

    // Update MainWindow (which handles all modules including Pomodoro)
    if (m_mainWindow)
        m_mainWindow->Update(deltaTime);

    // Update SettingsWindow if needed
    if (m_settingsWindow)
        m_settingsWindow->Update(deltaTime);
}

void UIManager::Render()
{
    if (!m_isInitialized)
        return;

    // Clear background
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render MainWindow (which contains all the UI including Pomodoro)
    if (m_mainWindow)
        m_mainWindow->Render();

    // Render additional windows if needed
    if (m_settingsWindow)
        m_settingsWindow->Render();

    // Render ImGui
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Present
    SwapBuffers(m_hdc);
}


void UIManager::ShowWindow() {
  if (!m_hwnd)
    return;

  if (IsIconic(m_hwnd)) {
    // If minimized → restore
    ::ShowWindow(m_hwnd, SW_RESTORE);
  } else {
    // If already visible → ensure it’s shown
    ::ShowWindow(m_hwnd, SW_SHOW);
  }

  // Force bring to foreground
  ForceToForeground(m_hwnd);

  // Backup calls (just in case)
  ::BringWindowToTop(m_hwnd);
  ::SetForegroundWindow(m_hwnd);

  m_isVisible = true;
}


//void UIManager::ShowWindow() {
//  if (m_hwnd) {
//    ::ShowWindow(m_hwnd,
//                 SW_RESTORE); // use RESTORE instead of SHOW for minimized case
//    ::ShowWindow(m_hwnd, SW_SHOW); // ensure visible if hidden
//    SetForegroundWindow(m_hwnd);
//    m_isVisible = true;
//  }
//}


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
    return m_isVisible && m_hwnd && ::IsWindowVisible(m_hwnd);
}

void UIManager::ForceToForeground(HWND hwnd) {
  DWORD foreThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
  DWORD appThread = GetCurrentThreadId();

  if (foreThread != appThread) {
    AttachThreadInput(foreThread, appThread, TRUE);
    SetForegroundWindow(hwnd);
    AttachThreadInput(foreThread, appThread, FALSE);
  } else {
    SetForegroundWindow(hwnd);
  }
}

void UIManager::SetWindowInfo(const WindowInfo& info)
{
    m_windowInfo = info;
    
    if (m_hwnd)
    {
        if (info.maximized)
        {
            ::ShowWindow(m_hwnd, SW_MAXIMIZE);
        }
        else
        {
            ::SetWindowPos(m_hwnd, nullptr, info.x, info.y, info.width, info.height,
                          SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
}

UIManager::WindowInfo UIManager::GetWindowInfo() const
{
    WindowInfo info = m_windowInfo;
    
    if (m_hwnd)
    {
        RECT rect;
        if (GetWindowRect(m_hwnd, &rect))
        {
            info.x = rect.left;
            info.y = rect.top;
            info.width = rect.right - rect.left;
            info.height = rect.bottom - rect.top;
        }
        
        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);
        if (GetWindowPlacement(m_hwnd, &wp))
        {
            info.maximized = (wp.showCmd == SW_MAXIMIZE);
        }
    }
    
    return info;
}

bool UIManager::CreateMainWindow(HINSTANCE hInstance)
{
    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = m_hIcon;
    wc.hIconSm = m_hIconSmall;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hIconSm = LoadIcon(hInstance, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        Logger::Error("Failed to register window class");
        return false;
    }

    // Create window
    m_hwnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        WINDOW_CLASS_NAME,
        "Potensio - Productivity Suite",
        WS_OVERLAPPEDWINDOW,
        m_windowInfo.x, m_windowInfo.y,
        m_windowInfo.width, m_windowInfo.height,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hwnd)
    {
        Logger::Error("Failed to create main window");
        return false;
    }

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
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
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

    return true;
}

bool UIManager::InitializeImGui()
{
    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Basic ImGui without docking or viewports

    // Platform/Renderer backends
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

    return true;
}

void UIManager::SetupImGuiStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Colors
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    
    // Rounding
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.TabRounding = 3.0f;
}

LRESULT CALLBACK UIManager::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_CREATE)
    {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        UIManager* pUIManager = reinterpret_cast<UIManager*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pUIManager));
        return 0;
    }

    UIManager* pUIManager = reinterpret_cast<UIManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (pUIManager)
    {
        return pUIManager->HandleMessage(hwnd, uMsg, wParam, lParam);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT UIManager::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam,
                                 LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
    return true;

  switch (uMsg) {
  case WM_DROPFILES: {
    HDROP hDrop = (HDROP)wParam;
    UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

    for (UINT i = 0; i < fileCount; i++) {
      char filePath[MAX_PATH];
      DragQueryFile(hDrop, i, filePath, MAX_PATH);
      if (m_mainWindow)
        m_mainWindow->AddStagedFile(filePath);
    }
    DragFinish(hDrop);

    // Start shake detection
    m_draggingFile = true;
    m_lastDirection = 0;
    m_shakeCount = 0;
    m_lastShakeTime = GetTickCount();
    GetCursorPos(&m_lastMousePos);
    return 0;
  }

  case WM_MOUSEMOVE: {
    if (m_draggingFile) {
      POINT p;
      GetCursorPos(&p);
      int dx = p.x - m_lastMousePos.x;

      if (abs(dx) > 20) // movement threshold
      {
        int direction = (dx > 0) ? 1 : -1;
        DWORD now = GetTickCount();

        // Reset if too much time has passed
        if (now - m_lastShakeTime > 1000) {
          m_shakeCount = 0;
        }

        // Count direction change
        if (m_lastDirection != 0 && direction != m_lastDirection) {
          m_shakeCount++;
          m_lastShakeTime = now;
        }

        m_lastDirection = direction;
        m_lastMousePos = p;

        // Trigger after 4 shakes
        if (m_shakeCount >= 4) {
          ShowWindow(); // your wrapper
          m_draggingFile = false;
        }
      }
    }
    break;
  }

  case WM_LBUTTONUP: // drag ended
  {
    m_draggingFile = false;
    break;
  }

  case WM_CLOSE:
    HideWindow();
    return 0;

  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;

  case WM_SHOW_POTENSIO:
    ShowWindow();
    return 0;
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
