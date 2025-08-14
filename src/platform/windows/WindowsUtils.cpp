// WindowsUtils.cpp - FIXED VERSION (renamed GetTempPath to GetSystemTempPath)
#include "WindowsUtils.h"
#include "core/Logger.h"

// Windows headers - order matters!
#include <windows.h>
#include <shlobj.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <shellapi.h>

#include <sstream>
#include <iomanip>

// System information
std::string WindowsUtils::GetWindowsVersion()
{
    OSVERSIONINFOEXA osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXA));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
    
#pragma warning(push)
#pragma warning(disable: 4996) // Suppress deprecation warning
    if (GetVersionExA(reinterpret_cast<OSVERSIONINFOA*>(&osvi)))
#pragma warning(pop)
    {
        std::ostringstream oss;
        oss << "Windows " << osvi.dwMajorVersion << "." << osvi.dwMinorVersion;
        oss << " Build " << osvi.dwBuildNumber;
        
        if (osvi.szCSDVersion[0] != '\0')
        {
            oss << " " << osvi.szCSDVersion;
        }
        
        return oss.str();
    }
    
    return "Unknown Windows Version";
}

std::string WindowsUtils::GetComputerName()
{
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);
    
    if (::GetComputerNameA(computerName, &size))
    {
        return std::string(computerName);
    }
    
    return "Unknown Computer";
}

std::string WindowsUtils::GetUserName()
{
    char userName[256];
    DWORD size = sizeof(userName);
    
    if (::GetUserNameA(userName, &size))
    {
        return std::string(userName);
    }
    
    return "Unknown User";
}

uint64_t WindowsUtils::GetTotalMemory()
{
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    
    if (GlobalMemoryStatusEx(&memInfo))
    {
        return memInfo.ullTotalPhys;
    }
    
    return 0;
}

uint64_t WindowsUtils::GetAvailableMemory()
{
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    
    if (GlobalMemoryStatusEx(&memInfo))
    {
        return memInfo.ullAvailPhys;
    }
    
    return 0;
}

// Process management
DWORD WindowsUtils::GetCurrentProcessId()
{
    return ::GetCurrentProcessId();
}

bool WindowsUtils::IsProcessRunning(const std::string& processName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;
        
    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32);
    
    bool found = false;
    if (Process32First(snapshot, &processEntry))
    {
        do
        {
            std::string exeFile(processEntry.szExeFile);
            if (processName == exeFile)
            {
                found = true;
                break;
            }
        } while (Process32Next(snapshot, &processEntry));
    }
    
    CloseHandle(snapshot);
    return found;
}

bool WindowsUtils::SetStartupRegistryEntry(const std::string& appName, const std::string& appPath, bool enable)
{
    const std::string regKey = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
    
    if (enable)
    {
        return WriteRegistryString(HKEY_CURRENT_USER, regKey, appName, appPath);
    }
    else
    {
        return DeleteRegistryValue(HKEY_CURRENT_USER, regKey, appName);
    }
}

// Window management
bool WindowsUtils::SetWindowTopMost(HWND hwnd, bool topMost)
{
    HWND insertAfter = topMost ? HWND_TOPMOST : HWND_NOTOPMOST;
    return SetWindowPos(hwnd, insertAfter, 0, 0, 0, 0, 
                       SWP_NOMOVE | SWP_NOSIZE) != FALSE;
}

bool WindowsUtils::CenterWindow(HWND hwnd)
{
    RECT windowRect, screenRect;
    
    if (!GetWindowRect(hwnd, &windowRect))
        return false;
        
    screenRect.left = 0;
    screenRect.top = 0;
    screenRect.right = GetSystemMetrics(SM_CXSCREEN);
    screenRect.bottom = GetSystemMetrics(SM_CYSCREEN);
    
    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;
    int screenWidth = screenRect.right - screenRect.left;
    int screenHeight = screenRect.bottom - screenRect.top;
    
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;
    
    return SetWindowPos(hwnd, nullptr, x, y, 0, 0, 
                       SWP_NOSIZE | SWP_NOZORDER) != FALSE;
}

bool WindowsUtils::AnimateWindow(HWND hwnd, bool show, DWORD duration)
{
    DWORD flags = show ? AW_ACTIVATE : AW_HIDE;
    flags |= AW_BLEND; // Fade effect
    
    return ::AnimateWindow(hwnd, duration, flags) != FALSE;
}

// Registry utilities
bool WindowsUtils::WriteRegistryString(HKEY root, const std::string& subKey, const std::string& valueName, const std::string& value)
{
    HKEY hKey;
    LONG result = RegOpenKeyExA(root, subKey.c_str(), 0, KEY_SET_VALUE, &hKey);
    
    if (result != ERROR_SUCCESS)
    {
        result = RegCreateKeyExA(root, subKey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
                                KEY_SET_VALUE, nullptr, &hKey, nullptr);
    }
    
    if (result == ERROR_SUCCESS)
    {
        result = RegSetValueExA(hKey, valueName.c_str(), 0, REG_SZ,
                               reinterpret_cast<const BYTE*>(value.c_str()),
                               static_cast<DWORD>(value.length() + 1));
        RegCloseKey(hKey);
    }
    
    return result == ERROR_SUCCESS;
}

std::string WindowsUtils::ReadRegistryString(HKEY root, const std::string& subKey, const std::string& valueName, const std::string& defaultValue)
{
    HKEY hKey;
    LONG result = RegOpenKeyExA(root, subKey.c_str(), 0, KEY_READ, &hKey);
    
    if (result != ERROR_SUCCESS)
        return defaultValue;
        
    char buffer[1024];
    DWORD bufferSize = sizeof(buffer);
    DWORD dataType;
    
    result = RegQueryValueExA(hKey, valueName.c_str(), nullptr, &dataType,
                             reinterpret_cast<LPBYTE>(buffer), &bufferSize);
    
    RegCloseKey(hKey);
    
    if (result == ERROR_SUCCESS && dataType == REG_SZ)
    {
        return std::string(buffer);
    }
    
    return defaultValue;
}

bool WindowsUtils::DeleteRegistryValue(HKEY root, const std::string& subKey, const std::string& valueName)
{
    HKEY hKey;
    LONG result = RegOpenKeyExA(root, subKey.c_str(), 0, KEY_SET_VALUE, &hKey);
    
    if (result == ERROR_SUCCESS)
    {
        result = RegDeleteValueA(hKey, valueName.c_str());
        RegCloseKey(hKey);
    }
    
    return result == ERROR_SUCCESS;
}

// File system
std::string WindowsUtils::GetSpecialFolderPath(int folderId)
{
    char path[MAX_PATH];
    
    if (SUCCEEDED(SHGetFolderPathA(nullptr, folderId, nullptr, 0, path)))
    {
        return std::string(path);
    }
    
    return "";
}

std::string WindowsUtils::GetApplicationDataPath()
{
    return GetSpecialFolderPath(CSIDL_APPDATA);
}

std::string WindowsUtils::GetSystemTempPath()
{
    // RENAMED: GetTempPath -> GetSystemTempPath to avoid macro conflicts
    char tempPath[MAX_PATH];
    DWORD result = ::GetTempPathA(MAX_PATH, tempPath);
    
    if (result > 0 && result < MAX_PATH)
    {
        return std::string(tempPath);
    }
    
    return "";
}

bool WindowsUtils::OpenFileInExplorer(const std::string& filepath)
{
    std::string command = "explorer.exe /select,\"" + filepath + "\"";
    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    
    si.cb = sizeof(si);
    
    bool result = CreateProcessA(nullptr, const_cast<char*>(command.c_str()),
                                nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi) != FALSE;
    
    if (result)
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    
    return result;
}

bool WindowsUtils::OpenUrlInBrowser(const std::string& url)
{
    return reinterpret_cast<INT_PTR>(ShellExecuteA(nullptr, "open", url.c_str(), 
                                                  nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}

// Notifications
bool WindowsUtils::ShowBalloonNotification(const std::string& title, const std::string& message, DWORD /*timeout*/)
{
    // This is a simplified implementation
    // In a real app, you'd use the system tray icon for notifications
    Logger::Info("Notification: {} - {}", title, message);
    return true;
}

// Clipboard
bool WindowsUtils::SetClipboardText(const std::string& text)
{
    if (!OpenClipboard(nullptr))
        return false;
        
    if (!EmptyClipboard())
    {
        CloseClipboard();
        return false;
    }
    
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, text.length() + 1);
    if (!hGlobal)
    {
        CloseClipboard();
        return false;
    }
    
    char* pGlobal = static_cast<char*>(GlobalLock(hGlobal));
    strcpy_s(pGlobal, text.length() + 1, text.c_str());
    GlobalUnlock(hGlobal);
    
    bool success = SetClipboardData(CF_TEXT, hGlobal) != nullptr;
    
    CloseClipboard();
    
    if (!success)
    {
        GlobalFree(hGlobal);
    }
    
    return success;
}

std::string WindowsUtils::GetClipboardText()
{
    if (!OpenClipboard(nullptr))
        return "";
        
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (!hData)
    {
        CloseClipboard();
        return "";
    }
    
    char* pData = static_cast<char*>(GlobalLock(hData));
    if (!pData)
    {
        CloseClipboard();
        return "";
    }
    
    std::string result(pData);
    
    GlobalUnlock(hData);
    CloseClipboard();
    
    return result;
}

bool WindowsUtils::IsClipboardFormatAvailable(UINT format)
{
    return ::IsClipboardFormatAvailable(format) != FALSE;
}

// Error handling
std::string WindowsUtils::GetLastErrorString()
{
    return GetErrorString(GetLastError());
}

std::string WindowsUtils::GetErrorString(DWORD errorCode)
{
    if (errorCode == 0)
        return "No error";
        
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&messageBuffer),
        0,
        nullptr
    );
    
    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    
    return message;
}