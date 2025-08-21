#pragma once

#include <chrono>
#include <functional>
#include <vector>

class PomodoroTimer
{
public:
    enum class TimerState
    {
        Stopped,
        Running,
        Paused,
        WorkSession,
        RestSession
    };

    enum class SessionType
    {
        Work,
        ShortBreak,
        LongBreak
    };

    struct NotifyPomodoro {
      bool hasNotify10;
      bool hasNotify50;
      bool hasNotify90;
      bool hasNotifyTimeup;
      bool hasNotifyStart;
    };

    struct PomodoroConfig
    {
        int workDurationMinutes = 25;
        int shortBreakMinutes = 5;
        int longBreakMinutes = 15;
        int sessionsBeforeLongBreak = 4;
        int totalSessions = 8;
        bool autoStartNextSession = true; // New setting for auto-start
        
        // Color settings (RGB 0-255)
        struct ColorRange
        {
            float r, g, b;
        };
        
        ColorRange colorHigh = {0.0f, 1.0f, 0.0f};    // >90% - Green
        ColorRange colorMedium = {1.0f, 1.0f, 0.0f};  // 50-90% - Yellow
        ColorRange colorLow = {1.0f, 0.5f, 0.0f};     // 10-50% - Orange
        ColorRange colorCritical = {1.0f, 0.0f, 0.0f}; // <10% - Red
    };

    struct SessionInfo
    {
        SessionType type;
        int sessionNumber;
        int totalSessions;
        std::chrono::seconds duration;
        std::chrono::seconds remaining;
        std::chrono::seconds pausedTime;
        bool isActive;
        NotifyPomodoro notify;
    };

public:
    PomodoroTimer();
    ~PomodoroTimer() = default;

    // Core timer functions
    void Start();
    void Pause();
    void Resume();
    void Stop();
    void Reset();
    void Skip();

    // Configuration
    void SetConfig(const PomodoroConfig& config);
    const PomodoroConfig& GetConfig() const { return m_config; }

    // State queries
    TimerState GetState() const { return m_state; }
    NotifyPomodoro& GetNotifications();
    SessionInfo GetCurrentSession() const;
    SessionType& GetCurrentSessionType();
    float GetProgressPercentage() const;
    std::string GetFormattedTimeRemaining() const;
    std::string GetSessionDescription() const;
    
    // Color based on progress
    PomodoroConfig::ColorRange GetCurrentColor() const;
    
    // Statistics
    int GetCompletedSessions() const { return m_completedSessions; }
    std::chrono::seconds GetTotalPausedTime() const { return m_totalPausedTime; }
    std::chrono::seconds GetSessionPausedTime() const { return m_sessionPausedTime; }

    // Callbacks
    void SetOnSessionComplete(std::function<void(SessionType)> callback) { m_onSessionComplete = callback; }
    void SetOnAllSessionsComplete(std::function<void()> callback) { m_onAllSessionsComplete = callback; }
    void SetOnTick(std::function<void()> callback) { m_onTick = callback; }

    // Update function - call this regularly
    void Update();

private:
    void StartNewSession();
    void CompleteCurrentSession();
    SessionType GetNextSessionType() const;
    std::chrono::seconds GetSessionDuration(SessionType type) const;
    void TriggerCallbacks();

private:
    PomodoroConfig m_config;
    TimerState m_state;
    SessionType m_currentSessionType;
    NotifyPomodoro m_notifications; // To prevent send notification on each render
    
    int m_currentSession;
    int m_completedSessions;
    
    std::chrono::steady_clock::time_point m_sessionStartTime;
    std::chrono::steady_clock::time_point m_pauseStartTime;
    std::chrono::seconds m_sessionDuration;
    std::chrono::seconds m_sessionPausedTime;
    std::chrono::seconds m_totalPausedTime;
    
    // Callbacks
    std::function<void(SessionType)> m_onSessionComplete;
    std::function<void()> m_onAllSessionsComplete;
    std::function<void()> m_onTick;
    
    // For tracking updates
    std::chrono::steady_clock::time_point m_lastUpdateTime;
};