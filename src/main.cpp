// src/main.cpp
#include <windows.h>
#include "app/Application.h"
#include "core/Logger.h"
#include <tchar.h>

// #include <winrt/Windows.Foundation.h>
// #include <winrt/Windows.UI.Notifications.h>
#include <shobjidl.h>  
#include <propvarutil.h>  
#include <propkey.h>  
#include <shobjidl_core.h>

#include "winsparkle.h"

// Mutex the app
#define POTENSIO_MUTEX_NAME _T("Potensio_SingleInstance_Mutex")
#define WM_SHOW_POTENSIO    (WM_USER + 1)

HWND FindPotensioWindow()
{
    return FindWindow(_T("PotensioMainWindow"), NULL); // Match the name in UIManager
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
{

    // Create named mutex to enforce single instance

    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wunused-variable"
    HANDLE hMutex = CreateMutex(NULL, FALSE, POTENSIO_MUTEX_NAME);
    #pragma clang diagnostic pop

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        // Another instance running — bring it to front
        HWND hwnd = FindPotensioWindow();
        if (hwnd)
        {
            PostMessage(hwnd, WM_SHOW_POTENSIO, 0, 0);
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        }
        return 0;
    }

    // Initialize logger
    Logger::Initialize();
    Logger::Info("Potensio starting up...");

    // Initialize WinSparkle
    win_sparkle_set_appcast_url("https://github.com/alviansm/Potensio-CE/releases/latest/download/appcast.xml");
    win_sparkle_set_app_details(L"AlviansMaulana", L"PotensioCE", L"0.1.1");
    win_sparkle_init();

    try 
    {
        Application app;
        if (!app.Initialize(hInstance))
        {
            Logger::Error("Failed to initialize application");
            return -1;
        }

        app.ShowMainWindow();

        int result = app.Run();
        Logger::Info("Potensio shutting down...");
        return result;
    }
    catch (const std::exception& e)
    {
        Logger::Error("Unhandled exception: {}", e.what());
        MessageBoxA(nullptr, e.what(), "Potensio - Fatal Error", MB_ICONERROR);
        return -1;
    }
    catch (...)
    {
        Logger::Error("Unknown exception occurred");
        MessageBoxA(nullptr, "An unknown error occurred", "Potensio - Fatal Error", MB_ICONERROR);
        return -1;
    }

     // Cleanup on exit
    win_sparkle_cleanup();
}

// Console entry point for debug builds
int main()
{
    return WinMain(GetModuleHandle(nullptr), nullptr, GetCommandLineA(), SW_SHOWNORMAL);
}