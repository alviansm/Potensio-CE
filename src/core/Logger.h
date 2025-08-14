// src/core/Logger.h
#pragma once

#include <string>
#include <iostream>
#include <sstream>

class Logger
{
public:
    enum class Level
    {
        Debug = 0,
        Info,
        Warning,
        Error
    };

    // Initialize logger
    static void Initialize();
    
    // Shutdown logger
    static void Shutdown();
    
    // Logging functions
    static void Debug(const std::string& message);
    static void Info(const std::string& message);
    static void Warning(const std::string& message);
    static void Error(const std::string& message);
    
    // Template functions for formatted logging
    template<typename... Args>
    static void Debug(const std::string& format, Args&&... args)
    {
        Log(Level::Debug, FormatString(format, std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    static void Info(const std::string& format, Args&&... args)
    {
        Log(Level::Info, FormatString(format, std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    static void Warning(const std::string& format, Args&&... args)
    {
        Log(Level::Warning, FormatString(format, std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    static void Error(const std::string& format, Args&&... args)
    {
        Log(Level::Error, FormatString(format, std::forward<Args>(args)...));
    }

private:
    static void Log(Level level, const std::string& message);
    static std::string GetLevelString(Level level);
    static std::string GetTimestamp();
    
    // Simple string formatting (C++17 compatible)
    template<typename T>
    static std::string FormatString(const std::string& format, T&& value)
    {
        std::string result = format;
        size_t pos = result.find("{}");
        if (pos != std::string::npos)
        {
            std::ostringstream oss;
            oss << value;
            result.replace(pos, 2, oss.str());
        }
        return result;
    }
    
    template<typename T, typename... Args>
    static std::string FormatString(const std::string& format, T&& value, Args&&... args)
    {
        std::string result = format;
        size_t pos = result.find("{}");
        if (pos != std::string::npos)
        {
            std::ostringstream oss;
            oss << value;
            result.replace(pos, 2, oss.str());
            return FormatString(result, std::forward<Args>(args)...);
        }
        return result;
    }
    
    static bool s_initialized;
    static Level s_minLevel;
};