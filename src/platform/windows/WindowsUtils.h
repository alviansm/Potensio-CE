#pragma once

#include <windows.h>
#include <string>

class WindowsUtils
{
public:
    // System information
    static std::string GetWindowsVersion();
    static std::string GetComputerName();
    static std::string GetUserName();
    static uint64_t GetTotalMemory();
    static uint64_t GetAvailableMemory();
    
    // Process management
    static DWORD GetCurrentProcessId();
    static bool IsProcessRunning(const std::string& processName);
    static bool SetStartupRegistryEntry(const std::string& appName, const std::string& appPath, bool enable);
    
    // Window management
    static bool SetWindowTopMost(HWND hwnd, bool topMost);
    static bool CenterWindow(HWND hwnd);
    static bool AnimateWindow(HWND hwnd, bool show, DWORD duration = 200);
    
    // Registry utilities
    static bool WriteRegistryString(HKEY root, const std::string& subKey, const std::string& valueName, const std::string& value);
    static std::string ReadRegistryString(HKEY root, const std::string& subKey, const std::string& valueName, const std::string& defaultValue = "");
    static bool DeleteRegistryValue(HKEY root, const std::string& subKey, const std::string& valueName);
    
    // File system
    static std::string GetSpecialFolderPath(int folderId);
    static std::string GetApplicationDataPath();
    static std::string GetSystemTempPath();  // RENAMED: GetTempPath -> GetSystemTempPath
    static bool OpenFileInExplorer(const std::string& filepath);
    static bool OpenUrlInBrowser(const std::string& url);
    
    // Notifications
    static bool ShowBalloonNotification(const std::string& title, const std::string& message, DWORD timeout = 5000);
    
    // Clipboard
    static bool SetClipboardText(const std::string& text);
    static std::string GetClipboardText();
    static bool IsClipboardFormatAvailable(UINT format);
    
    // Error handling
    static std::string GetLastErrorString();
    static std::string GetErrorString(DWORD errorCode);

private:
    WindowsUtils() = delete;
    ~WindowsUtils() = delete;
};