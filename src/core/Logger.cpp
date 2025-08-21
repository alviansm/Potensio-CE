// src/core/Logger.cpp - FIXED VERSION (Safe localtime)
#include "Logger.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <windows.h>

bool Logger::s_initialized = false;
Logger::Level Logger::s_minLevel = Logger::Level::Debug;

void Logger::Initialize()
{
    if (s_initialized)
        return;
        
    s_initialized = true;
    
    // Create logs directory if it doesn't exist
    CreateDirectoryA("logs", nullptr);
    
    Info("Logger initialized");
}

void Logger::Shutdown()
{
    if (!s_initialized)
        return;
        
    Info("Logger shutting down");
    s_initialized = false;
}

void Logger::Debug(const std::string& message)
{
    Log(Level::Debug, message);
}

void Logger::Info(const std::string& message)
{
    Log(Level::Info, message);
}

void Logger::Warning(const std::string& message)
{
    Log(Level::Warning, message);
}

void Logger::Error(const std::string& message)
{
    Log(Level::Error, message);
}

void Logger::Log(Level level, const std::string& message)
{
    if (!s_initialized || level < s_minLevel)
        return;
        
    std::string timestamp = GetTimestamp();
    std::string levelStr = GetLevelString(level);
    
    // Format: [TIMESTAMP] [LEVEL] MESSAGE
    std::string logLine = "[" + timestamp + "] [" + levelStr + "] " + message;
    
    // Output to console
    std::cout << logLine << std::endl;
    
    // Also output to debug console in Visual Studio
    OutputDebugStringA((logLine + "\n").c_str());
    
    // Could also log to file here if needed
    
    std::ofstream logFile("logs/potensio.log", std::ios::app);
    if (logFile.is_open())
    {
        logFile << logLine << std::endl;
        logFile.close();
    }
    
}

std::string Logger::GetLevelString(Level level)
{
    switch (level)
    {
        case Level::Debug:   return "DEBUG";
        case Level::Info:    return "INFO ";
        case Level::Warning: return "WARN ";
        case Level::Error:   return "ERROR";
        default:             return "UNKN ";
    }
}

std::string Logger::GetTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    // Use safe version of localtime
    struct tm timeinfo;
    if (localtime_s(&timeinfo, &time_t) == 0)
    {
        std::stringstream ss;
        ss << std::put_time(&timeinfo, "%H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }
    
    // Fallback if localtime_s fails
    return "??:??:??.???";
}