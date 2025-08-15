#include "PomodoroTimer.h"
#include "core/Logger.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

PomodoroTimer::PomodoroTimer()
    : m_state(TimerState::Stopped)
    , m_currentSessionType(SessionType::Work)
    , m_currentSession(1)
    , m_completedSessions(0)
    , m_sessionDuration(std::chrono::minutes(25))
    , m_sessionPausedTime(std::chrono::seconds(0))
    , m_totalPausedTime(std::chrono::seconds(0))
{
    m_lastUpdateTime = std::chrono::steady_clock::now();
    Logger::Debug("PomodoroTimer initialized");
}

void PomodoroTimer::Start()
{
    if (m_state == TimerState::Stopped)
    {
        // Only reset to session 1 if we're starting fresh (no completed sessions)
        if (m_completedSessions == 0)
        {
            m_currentSession = 1;
            m_completedSessions = 0;
            m_totalPausedTime = std::chrono::seconds(0);
            m_currentSessionType = SessionType::Work;
        }
        // If we have completed sessions, we're continuing from where we left off
        // (when auto-start was disabled and session completed)
        
        StartNewSession();
    }
    else if (m_state == TimerState::Paused)
    {
        Resume();
    }
    
    Logger::Info("Pomodoro timer started - Session {}/{}", m_currentSession, m_config.totalSessions);
}

void PomodoroTimer::Pause()
{
    if (m_state == TimerState::Running)
    {
        m_state = TimerState::Paused;
        m_pauseStartTime = std::chrono::steady_clock::now();
        Logger::Debug("Pomodoro timer paused");
    }
}

void PomodoroTimer::Resume()
{
    if (m_state == TimerState::Paused)
    {
        auto pauseDuration = std::chrono::steady_clock::now() - m_pauseStartTime;
        m_sessionPausedTime += std::chrono::duration_cast<std::chrono::seconds>(pauseDuration);
        m_totalPausedTime += std::chrono::duration_cast<std::chrono::seconds>(pauseDuration);
        
        m_state = TimerState::Running;
        Logger::Debug("Pomodoro timer resumed");
    }
}

void PomodoroTimer::Stop()
{
    m_state = TimerState::Stopped;
    
    // Reset everything when user explicitly stops
    m_currentSession = 1;
    m_completedSessions = 0;
    m_currentSessionType = SessionType::Work;
    m_sessionDuration = GetSessionDuration(m_currentSessionType);
    m_sessionPausedTime = std::chrono::seconds(0);
    m_totalPausedTime = std::chrono::seconds(0);
    
    // Reset timing variables for clean display
    m_sessionStartTime = std::chrono::steady_clock::now();
    
    Logger::Info("Pomodoro timer stopped and reset");
}

void PomodoroTimer::Reset()
{
    TimerState previousState = m_state;
    Stop();
    if (previousState != TimerState::Stopped)
    {
        Start();
    }
    Logger::Debug("Pomodoro timer reset");
}

void PomodoroTimer::Skip()
{
    if (m_state == TimerState::Running || m_state == TimerState::Paused)
    {
        CompleteCurrentSession();
        Logger::Debug("Pomodoro session skipped");
    }
}

void PomodoroTimer::SetConfig(const PomodoroConfig& config)
{
    m_config = config;
    
    // If timer is stopped, reset to use new config
    if (m_state == TimerState::Stopped)
    {
        m_currentSession = 1;
        m_completedSessions = 0;
    }
    
    Logger::Debug("Pomodoro configuration updated");
}

PomodoroTimer::SessionInfo PomodoroTimer::GetCurrentSession() const
{
    SessionInfo info;
    info.type = m_currentSessionType;
    info.sessionNumber = m_currentSession;
    info.totalSessions = m_config.totalSessions;
    info.duration = m_sessionDuration;
    info.pausedTime = m_sessionPausedTime;
    info.isActive = (m_state == TimerState::Running || m_state == TimerState::Paused);
    
    if (m_state == TimerState::Running)
    {
        // Normal running - calculate time remaining
        auto elapsed = std::chrono::steady_clock::now() - m_sessionStartTime - m_sessionPausedTime;
        auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed);
        info.remaining = m_sessionDuration - elapsedSeconds;
        
        if (info.remaining.count() < 0)
            info.remaining = std::chrono::seconds(0);
    }
    else if (m_state == TimerState::Paused)
    {
        // When paused, calculate time remaining at the moment of pause
        auto elapsed = m_pauseStartTime - m_sessionStartTime - m_sessionPausedTime;
        auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed);
        info.remaining = m_sessionDuration - elapsedSeconds;
        
        if (info.remaining.count() < 0)
            info.remaining = std::chrono::seconds(0);
    }
    else // TimerState::Stopped
    {
        // When stopped, show full duration for the current session type
        info.remaining = m_sessionDuration;
    }
    
    return info;
}

float PomodoroTimer::GetProgressPercentage() const
{
    if (m_state == TimerState::Stopped)
        return 1.0f;
    
    auto sessionInfo = GetCurrentSession();
    if (sessionInfo.duration.count() == 0)
        return 0.0f;
    
    float progress = static_cast<float>(sessionInfo.remaining.count()) / static_cast<float>(sessionInfo.duration.count());
    return std::max(0.0f, std::min(1.0f, progress));
}

std::string PomodoroTimer::GetFormattedTimeRemaining() const
{
    auto sessionInfo = GetCurrentSession();
    auto totalSeconds = sessionInfo.remaining.count();
    
    int minutes = static_cast<int>(totalSeconds / 60);
    int seconds = static_cast<int>(totalSeconds % 60);
    
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << minutes << ":" 
        << std::setfill('0') << std::setw(2) << seconds;
    return oss.str();
}

std::string PomodoroTimer::GetSessionDescription() const
{
    std::ostringstream oss;
    
    switch (m_currentSessionType)
    {
        case SessionType::Work:
            oss << "Focus Session";
            break;
        case SessionType::ShortBreak:
            oss << "Short Break";
            break;
        case SessionType::LongBreak:
            oss << "Long Break";
            break;
    }
    
    oss << " (" << m_currentSession << "/" << m_config.totalSessions << ")";
    return oss.str();
}

PomodoroTimer::PomodoroConfig::ColorRange PomodoroTimer::GetCurrentColor() const
{
    // If paused, return gray color
    if (m_state == TimerState::Paused)
    {
        PomodoroConfig::ColorRange grayColor;
        grayColor.r = 0.6f;
        grayColor.g = 0.6f;
        grayColor.b = 0.6f;
        return grayColor;
    }
    
    // If stopped, return a neutral color
    if (m_state == TimerState::Stopped)
    {
        PomodoroConfig::ColorRange neutralColor;
        neutralColor.r = 0.8f;
        neutralColor.g = 0.8f;
        neutralColor.b = 0.8f;
        return neutralColor;
    }
    
    // Normal color based on progress
    float progress = GetProgressPercentage();
    
    if (progress > 0.9f)
        return m_config.colorHigh;
    else if (progress > 0.5f)
        return m_config.colorMedium;
    else if (progress > 0.1f)
        return m_config.colorLow;
    else
        return m_config.colorCritical;
}

void PomodoroTimer::Update()
{
    auto currentTime = std::chrono::steady_clock::now();
    
    // Only update if we're in running state - STOP here if paused/stopped
    if (m_state != TimerState::Running)
    {
        m_lastUpdateTime = currentTime;
        return; // Don't update anything when paused or stopped
    }
    
    // Check if session should complete
    auto sessionInfo = GetCurrentSession();
    if (sessionInfo.remaining.count() <= 0)
    {
        CompleteCurrentSession();
    }
    
    // Trigger tick callback if enough time has passed (every second)
    auto timeSinceLastUpdate = currentTime - m_lastUpdateTime;
    if (timeSinceLastUpdate >= std::chrono::seconds(1))
    {
        TriggerCallbacks();
        m_lastUpdateTime = currentTime;
    }
}

void PomodoroTimer::StartNewSession()
{
    m_sessionStartTime = std::chrono::steady_clock::now();
    m_sessionPausedTime = std::chrono::seconds(0);
    m_sessionDuration = GetSessionDuration(m_currentSessionType);
    m_state = TimerState::Running;
    
    Logger::Info("Started {} - Duration: {} minutes", 
                GetSessionDescription(), 
                m_sessionDuration.count() / 60);
}

void PomodoroTimer::CompleteCurrentSession()
{
    Logger::Info("Completed {}", GetSessionDescription());
    
    // Trigger session complete callback
    if (m_onSessionComplete)
        m_onSessionComplete(m_currentSessionType);
    
    m_completedSessions++;
    
    // Check if all sessions are complete
    if (m_currentSession >= m_config.totalSessions)
    {
        m_state = TimerState::Stopped;
        Logger::Info("All Pomodoro sessions completed!");
        
        if (m_onAllSessionsComplete)
            m_onAllSessionsComplete();
        
        return;
    }
    
    // Move to next session
    m_currentSession++;
    m_currentSessionType = GetNextSessionType();
    
    // Check auto-start setting
    if (m_config.autoStartNextSession)
    {
        // Auto-start the next session
        StartNewSession();
        Logger::Info("Auto-started next session: {}", GetSessionDescription());
    }
    else
    {
        // Stop and prepare for manual start
        m_state = TimerState::Stopped;
        m_sessionDuration = GetSessionDuration(m_currentSessionType);
        
        // Reset session timing variables so timer displays correctly
        m_sessionStartTime = std::chrono::steady_clock::now();
        m_sessionPausedTime = std::chrono::seconds(0);
        
        Logger::Info("Session completed. Ready to start: {}", GetSessionDescription());
    }
}

PomodoroTimer::SessionType PomodoroTimer::GetNextSessionType() const
{
    if (m_currentSessionType == SessionType::Work)
    {
        // After work, determine break type
        if (m_completedSessions > 0 && m_completedSessions % m_config.sessionsBeforeLongBreak == 0)
            return SessionType::LongBreak;
        else
            return SessionType::ShortBreak;
    }
    else
    {
        // After any break, return to work
        return SessionType::Work;
    }
}

std::chrono::seconds PomodoroTimer::GetSessionDuration(SessionType type) const
{
    switch (type)
    {
        case SessionType::Work:
            return std::chrono::minutes(m_config.workDurationMinutes);
        case SessionType::ShortBreak:
            return std::chrono::minutes(m_config.shortBreakMinutes);
        case SessionType::LongBreak:
            return std::chrono::minutes(m_config.longBreakMinutes);
        default:
            return std::chrono::minutes(m_config.workDurationMinutes);
    }
}

void PomodoroTimer::TriggerCallbacks()
{
    if (m_onTick)
        m_onTick();
}