// src/core/Utils.h - FIXED VERSION
#pragma once

#include <string>
#include <vector>
#include <ctime>  // Added for time_t
#include <windows.h>
#include <shellapi.h>
#include <string>

class Utils
{
public:
    // File and directory utilities
    static bool FileExists(const std::string& path);
    static bool DirectoryExists(const std::string& path);
    static uint64_t GetFileSize(const std::string& path);
    static std::string GetFileName(const std::string& path);
    static std::string GetFileExtension(const std::string& path);
    static std::string GetDirectoryPath(const std::string& path);
    
    // String utilities
    static std::string FormatBytes(uint64_t bytes);
    static std::string ToLower(const std::string& str);
    static std::string ToUpper(const std::string& str);
    static std::string Trim(const std::string& str);
    static std::vector<std::string> Split(const std::string& str, char delimiter);
    
    // Path utilities
    static std::string JoinPath(const std::string& path1, const std::string& path2);
    static std::string GetAppDataPath();
    static std::string GetTempPath();
    
    // Time utilities
    static std::string GetCurrentTimeString();
    static std::string FormatTime(time_t time);
    
    // System utilities
    static void OpenFileInExplorer(const std::string& path);
    static void OpenUrl(const std::string& url);
    static bool CreateDirectoryRecursive(const std::string& path);
    static void ShowPasteCompleteNotification(const std::string& folderPath);
    
    // Encoding utilities
    static std::wstring UTF8ToWide(const std::string& utf8);
    static std::string WideToUTF8(const std::wstring& wide);
};