// src/core/Utils.cpp - FINAL VERSION (No warnings)
#include "Utils.h"
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>

// CRITICAL: Undefine Windows macros that conflict with our method names
#ifdef GetTempPath
#undef GetTempPath
#endif

#include <filesystem>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>

bool Utils::FileExists(const std::string& path)
{
    DWORD attributes = GetFileAttributesA(path.c_str());
    return (attributes != INVALID_FILE_ATTRIBUTES) && 
           !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool Utils::DirectoryExists(const std::string& path)
{
    DWORD attributes = GetFileAttributesA(path.c_str());
    return (attributes != INVALID_FILE_ATTRIBUTES) && 
           (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

uint64_t Utils::GetFileSize(const std::string& path)
{
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileInfo))
    {
        LARGE_INTEGER size;
        size.HighPart = fileInfo.nFileSizeHigh;
        size.LowPart = fileInfo.nFileSizeLow;
        return static_cast<uint64_t>(size.QuadPart);
    }
    return 0;
}

std::string Utils::GetFileName(const std::string& path)
{
    size_t lastSlash = path.find_last_of("\\/");
    if (lastSlash != std::string::npos)
    {
        return path.substr(lastSlash + 1);
    }
    return path;
}

std::string Utils::GetFileExtension(const std::string& path)
{
    std::string filename = GetFileName(path);
    size_t lastDot = filename.find_last_of('.');
    if (lastDot != std::string::npos && lastDot < filename.length() - 1)
    {
        return filename.substr(lastDot + 1);
    }
    return "";
}

std::string Utils::GetDirectoryPath(const std::string& path)
{
    size_t lastSlash = path.find_last_of("\\/");
    if (lastSlash != std::string::npos)
    {
        return path.substr(0, lastSlash);
    }
    return "";
}

std::string Utils::FormatBytes(uint64_t bytes)
{
    const char* suffixes[] = { "B", "KB", "MB", "GB", "TB" };
    const int numSuffixes = sizeof(suffixes) / sizeof(suffixes[0]);
    
    if (bytes == 0)
        return "0 B";
    
    int suffixIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && suffixIndex < numSuffixes - 1)
    {
        size /= 1024.0;
        suffixIndex++;
    }
    
    std::ostringstream oss;
    if (suffixIndex == 0)
    {
        oss << static_cast<uint64_t>(size) << " " << suffixes[suffixIndex];
    }
    else
    {
        oss << std::fixed << std::setprecision(1) << size << " " << suffixes[suffixIndex];
    }
    
    return oss.str();
}

std::string Utils::ToLower(const std::string& str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), 
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::string Utils::ToUpper(const std::string& str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), 
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

std::string Utils::Trim(const std::string& str)
{
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::vector<std::string> Utils::Split(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter))
    {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string Utils::JoinPath(const std::string& path1, const std::string& path2)
{
    if (path1.empty()) return path2;
    if (path2.empty()) return path1;
    
    char lastChar = path1.back();
    if (lastChar != '\\' && lastChar != '/')
    {
        return path1 + "\\" + path2;
    }
    
    return path1 + path2;
}

std::string Utils::GetAppDataPath()
{
    char* appData = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appData, &len, "APPDATA") == 0 && appData != nullptr)
    {
        std::string result(appData);
        free(appData);
        return JoinPath(result, "Potensio");
    }
    return "AppData";
}

std::string Utils::GetTempPath()
{
    char* temp = nullptr;
    size_t len = 0;
    
    if (_dupenv_s(&temp, &len, "TEMP") == 0 && temp != nullptr)
    {
        std::string result(temp);
        free(temp);
        return result;
    }
    
    if (_dupenv_s(&temp, &len, "TMP") == 0 && temp != nullptr)
    {
        std::string result(temp);
        free(temp);
        return result;
    }
    
    return "C:\\Windows\\Temp";
}

std::string Utils::GetCurrentTimeString()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    struct tm timeinfo;
    if (localtime_s(&timeinfo, &time_t) == 0)
    {
        std::stringstream ss;
        ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    return "Unknown Time";
}

std::string Utils::FormatTime(time_t time)
{
    struct tm timeinfo;
    if (localtime_s(&timeinfo, &time) == 0)
    {
        std::stringstream ss;
        ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    return "Unknown Time";
}

void Utils::OpenFileInExplorer(const std::string& path)
{
    std::string command = "explorer.exe /select,\"" + path + "\"";
    system(command.c_str());
}

void Utils::OpenUrl(const std::string& url)
{
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

bool Utils::CreateDirectoryRecursive(const std::string& path)
{
    try
    {
        std::filesystem::create_directories(path);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

std::wstring Utils::UTF8ToWide(const std::string& utf8)
{
    if (utf8.empty()) return std::wstring();
    
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (size == 0) return std::wstring();
    
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], size);
    
    return result;
}

std::string Utils::WideToUTF8(const std::wstring& wide)
{
    if (wide.empty()) return std::string();
    
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size == 0) return std::string();
    
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &result[0], size, nullptr, nullptr);
    
    return result;
}