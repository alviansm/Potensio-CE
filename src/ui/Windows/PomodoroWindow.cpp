#include "ui/Windows/PomodoroWindow.h"
#include "app/AppConfig.h"
#include "core/Logger.h"
#include <imgui.h>
#include <algorithm>

PomodoroWindow::PomodoroWindow()
    : m_timer(nullptr)
    , m_config(nullptr)
    , m_isVisible(false)
    , m_showAdvancedSettings(false)
    , m_showColorSettings(false)
    , m_showStatistics(false)
    , m_showNotificationSettings(false)
    , m_settingsChanged(false)
{
}

bool PomodoroWindow::Initialize(AppConfig* config)
{
    m_config = config;
    LoadSettings();
    
    Logger::Info("PomodoroWindow (Settings) initialized");
    return true;
}

void PomodoroWindow::Shutdown()
{
    SaveSettings();
    Logger::Debug("PomodoroWindow (Settings) shutdown");
}

void PomodoroWindow::Update(float /*deltaTime*/)  // Suppress unused parameter warning
{
    // Simple update for settings window
    // No complex animations needed for settings
}

void PomodoroWindow::Render()
{
    if (!m_isVisible)
        return;
    
    RenderSettingsWindow();
}

void PomodoroWindow::RenderSettingsWindow()
{
    ImGui::SetNextWindowSize(ImVec2(SETTINGS_WINDOW_WIDTH, SETTINGS_WINDOW_HEIGHT), ImGuiCond_FirstUseEver);
    
    bool windowOpen = m_isVisible;
    if (ImGui::Begin("Pomodoro Advanced Settings", &windowOpen, ImGuiWindowFlags_NoCollapse))
    {
        // Tab bar for different settings categories
        if (ImGui::BeginTabBar("SettingsTabs"))
        {
            if (ImGui::BeginTabItem("⏱ Timer"))
            {
                RenderAdvancedSettings();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("🎨 Colors"))
            {
                RenderColorSettings();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("📊 Statistics"))
            {
                RenderStatistics();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("🔔 Notifications"))
            {
                RenderNotificationSettings();
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
        
        ImGui::Separator();
        
        // Action buttons
        if (ImGui::Button("Apply & Save"))
        {
            if (m_timer)
                m_timer->SetConfig(m_tempConfig);
            SaveSettings();
            m_settingsChanged = false;
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Reset to Defaults"))
        {
            ResetToDefaults();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            LoadSettings(); // Reload from saved settings
            m_settingsChanged = false;
            m_isVisible = false;
        }
        
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - 50);
        if (ImGui::Button("Close"))
        {
            m_isVisible = false;
        }
    }
    else
    {
        windowOpen = false;
    }
    ImGui::End();
    
    m_isVisible = windowOpen;
}

void PomodoroWindow::RenderAdvancedSettings()
{
    if (ImGui::CollapsingHeader("Duration Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        RenderDurationSettings();
    }
    
    ImGui::Spacing();
    
    if (ImGui::CollapsingHeader("Session Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        RenderSessionSettings();
    }
    
    ImGui::Spacing();
    
    if (ImGui::CollapsingHeader("Behavior Settings"))
    {
        ImGui::Text("Auto-start next session:");
        ImGui::SameLine();
        bool autoStart = m_tempConfig.autoStartNextSession;
        if (ImGui::Checkbox("##autostart", &autoStart))
        {
            m_tempConfig.autoStartNextSession = autoStart;
            m_settingsChanged = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Automatically start the next session when current one ends.\nWhen disabled, timer stops and waits for manual start.");
        }
        
        ImGui::Text("Auto-pause on system idle:");
        ImGui::SameLine();
        static bool autoPause = false;
        ImGui::Checkbox("##autopause", &autoPause);
        
        ImGui::Text("Continue timer on app close:");
        ImGui::SameLine();
        static bool continueOnClose = true;
        ImGui::Checkbox("##continue", &continueOnClose);
    }
}

void PomodoroWindow::RenderColorSettings()
{
    ImGui::Text("Customize colors based on remaining time percentage:");
    ImGui::Spacing();
    
    RenderColorPicker("High (>90%)", m_tempConfig.colorHigh);
    ImGui::Text("Used when more than 90%% of time remains");
    ImGui::Spacing();
    
    RenderColorPicker("Medium (50-90%)", m_tempConfig.colorMedium);
    ImGui::Text("Used when 50-90%% of time remains");
    ImGui::Spacing();
    
    RenderColorPicker("Low (10-50%)", m_tempConfig.colorLow);
    ImGui::Text("Used when 10-50%% of time remains");
    ImGui::Spacing();
    
    RenderColorPicker("Critical (<10%)", m_tempConfig.colorCritical);
    ImGui::Text("Used when less than 10%% of time remains");
    ImGui::Spacing();
    
    ImGui::Separator();
    
    if (ImGui::Button("Preview Colors"))
    {
        // TODO: Show color preview
        Logger::Info("Color preview requested");
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Reset Colors"))
    {
        // Reset to default colors
        m_tempConfig.colorHigh = {0.0f, 1.0f, 0.0f};
        m_tempConfig.colorMedium = {1.0f, 1.0f, 0.0f};
        m_tempConfig.colorLow = {1.0f, 0.5f, 0.0f};
        m_tempConfig.colorCritical = {1.0f, 0.0f, 0.0f};
        m_settingsChanged = true;
    }
}

void PomodoroWindow::RenderStatistics()
{
    ImGui::Text("Session Statistics");
    ImGui::Separator();
    
    if (m_timer)
    {
        ImGui::Text("Today's Sessions: %d", m_timer->GetCompletedSessions());
        
        auto totalPaused = m_timer->GetTotalPausedTime();
        int pausedMinutes = static_cast<int>(totalPaused.count() / 60);
        int pausedSeconds = static_cast<int>(totalPaused.count() % 60);
        ImGui::Text("Total Paused Time: %02d:%02d", pausedMinutes, pausedSeconds);
        
        ImGui::Text("Current State: %s", 
                   m_timer->GetState() == PomodoroTimer::TimerState::Running ? "Running" :
                   m_timer->GetState() == PomodoroTimer::TimerState::Paused ? "Paused" :
                   m_timer->GetState() == PomodoroTimer::TimerState::Stopped ? "Stopped" : "Unknown");
    }
    else
    {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No timer data available");
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Additional statistics placeholders
    ImGui::Text("Historical Data (Coming Soon)");
    ImGui::BulletText("Daily/Weekly/Monthly summaries");
    ImGui::BulletText("Productivity trends");
    ImGui::BulletText("Session completion rates");
    ImGui::BulletText("Average session lengths");
    
    ImGui::Spacing();
    
    if (ImGui::Button("Export Statistics"))
    {
        Logger::Info("Export statistics requested");
        // TODO: Implement statistics export
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Clear Statistics"))
    {
        Logger::Info("Clear statistics requested");
        // TODO: Implement statistics clearing
    }
}

void PomodoroWindow::RenderNotificationSettings()
{
    ImGui::Text("Notification Settings");
    ImGui::Separator();
    
    static bool enableNotifications = true;
    ImGui::Checkbox("Enable notifications", &enableNotifications);
    
    if (enableNotifications)
    {
        static bool soundEnabled = true;
        ImGui::Checkbox("Play sound", &soundEnabled);
        
        static bool showPopup = true;
        ImGui::Checkbox("Show popup window", &showPopup);
        
        static bool systemTrayNotification = true;
        ImGui::Checkbox("System tray notification", &systemTrayNotification);
        
        ImGui::Spacing();
        
        ImGui::Text("Notification timing:");
        static int notificationTiming = 0;
        ImGui::RadioButton("On session complete", &notificationTiming, 0);
        ImGui::RadioButton("30 seconds before end", &notificationTiming, 1);
        ImGui::RadioButton("1 minute before end", &notificationTiming, 2);
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Text("Custom Messages");
    
    static char workCompleteMsg[256] = "Work session complete! Time for a break.";
    ImGui::InputText("Work complete message", workCompleteMsg, sizeof(workCompleteMsg));
    
    static char breakCompleteMsg[256] = "Break finished! Ready to focus?";
    ImGui::InputText("Break complete message", breakCompleteMsg, sizeof(breakCompleteMsg));
    
    ImGui::Spacing();
    
    if (ImGui::Button("Test Notification"))
    {
        Logger::Info("Test notification triggered");
        // TODO: Show test notification
    }
}

void PomodoroWindow::RenderDurationSettings()
{
    bool changed = false;
    
    changed |= ImGui::SliderInt("Work Duration (minutes)", &m_tempConfig.workDurationMinutes, 1, 120);
    changed |= ImGui::SliderInt("Short Break (minutes)", &m_tempConfig.shortBreakMinutes, 1, 30);
    changed |= ImGui::SliderInt("Long Break (minutes)", &m_tempConfig.longBreakMinutes, 5, 60);
    
    if (changed)
        m_settingsChanged = true;
}

void PomodoroWindow::RenderSessionSettings()
{
    bool changed = false;
    
    changed |= ImGui::SliderInt("Total Sessions", &m_tempConfig.totalSessions, 1, 20);
    changed |= ImGui::SliderInt("Sessions Before Long Break", &m_tempConfig.sessionsBeforeLongBreak, 2, 8);
    
    if (changed)
        m_settingsChanged = true;
}

void PomodoroWindow::RenderColorPicker(const char* label, PomodoroTimer::PomodoroConfig::ColorRange& color)
{
    float colorArray[3] = { color.r, color.g, color.b };
    
    if (ImGui::ColorEdit3(label, colorArray))
    {
        color.r = colorArray[0];
        color.g = colorArray[1];
        color.b = colorArray[2];
        m_settingsChanged = true;
    }
}

void PomodoroWindow::LoadSettings()
{
    if (!m_config)
        return;
    
    // Load from config or set defaults
    m_tempConfig.workDurationMinutes = m_config->GetValue("pomodoro.work_duration", 25);
    m_tempConfig.shortBreakMinutes = m_config->GetValue("pomodoro.short_break", 5);
    m_tempConfig.longBreakMinutes = m_config->GetValue("pomodoro.long_break", 15);
    m_tempConfig.totalSessions = m_config->GetValue("pomodoro.total_sessions", 8);
    m_tempConfig.sessionsBeforeLongBreak = m_config->GetValue("pomodoro.sessions_before_long_break", 4);
    m_tempConfig.autoStartNextSession = m_config->GetValue("pomodoro.auto_start_next", true);
    
    // Load colors
    m_tempConfig.colorHigh.r = m_config->GetValue("pomodoro.color_high_r", 0.0f);
    m_tempConfig.colorHigh.g = m_config->GetValue("pomodoro.color_high_g", 1.0f);
    m_tempConfig.colorHigh.b = m_config->GetValue("pomodoro.color_high_b", 0.0f);
    
    m_tempConfig.colorMedium.r = m_config->GetValue("pomodoro.color_medium_r", 1.0f);
    m_tempConfig.colorMedium.g = m_config->GetValue("pomodoro.color_medium_g", 1.0f);
    m_tempConfig.colorMedium.b = m_config->GetValue("pomodoro.color_medium_b", 0.0f);
    
    m_tempConfig.colorLow.r = m_config->GetValue("pomodoro.color_low_r", 1.0f);
    m_tempConfig.colorLow.g = m_config->GetValue("pomodoro.color_low_g", 0.5f);
    m_tempConfig.colorLow.b = m_config->GetValue("pomodoro.color_low_b", 0.0f);
    
    m_tempConfig.colorCritical.r = m_config->GetValue("pomodoro.color_critical_r", 1.0f);
    m_tempConfig.colorCritical.g = m_config->GetValue("pomodoro.color_critical_g", 0.0f);
    m_tempConfig.colorCritical.b = m_config->GetValue("pomodoro.color_critical_b", 0.0f);
    
    Logger::Debug("Pomodoro advanced settings loaded");
}

void PomodoroWindow::SaveSettings()
{
    if (!m_config)
        return;
    
    // Save all configuration values
    m_config->SetValue("pomodoro.work_duration", m_tempConfig.workDurationMinutes);
    m_config->SetValue("pomodoro.short_break", m_tempConfig.shortBreakMinutes);
    m_config->SetValue("pomodoro.long_break", m_tempConfig.longBreakMinutes);
    m_config->SetValue("pomodoro.total_sessions", m_tempConfig.totalSessions);
    m_config->SetValue("pomodoro.sessions_before_long_break", m_tempConfig.sessionsBeforeLongBreak);
    m_config->SetValue("pomodoro.auto_start_next", m_tempConfig.autoStartNextSession);
    
    // Save colors
    m_config->SetValue("pomodoro.color_high_r", m_tempConfig.colorHigh.r);
    m_config->SetValue("pomodoro.color_high_g", m_tempConfig.colorHigh.g);
    m_config->SetValue("pomodoro.color_high_b", m_tempConfig.colorHigh.b);
    
    m_config->SetValue("pomodoro.color_medium_r", m_tempConfig.colorMedium.r);
    m_config->SetValue("pomodoro.color_medium_g", m_tempConfig.colorMedium.g);
    m_config->SetValue("pomodoro.color_medium_b", m_tempConfig.colorMedium.b);
    
    m_config->SetValue("pomodoro.color_low_r", m_tempConfig.colorLow.r);
    m_config->SetValue("pomodoro.color_low_g", m_tempConfig.colorLow.g);
    m_config->SetValue("pomodoro.color_low_b", m_tempConfig.colorLow.b);
    
    m_config->SetValue("pomodoro.color_critical_r", m_tempConfig.colorCritical.r);
    m_config->SetValue("pomodoro.color_critical_g", m_tempConfig.colorCritical.g);
    m_config->SetValue("pomodoro.color_critical_b", m_tempConfig.colorCritical.b);
    
    m_config->Save();
    Logger::Debug("Pomodoro advanced settings saved");
}

void PomodoroWindow::ResetToDefaults()
{
    m_tempConfig = PomodoroTimer::PomodoroConfig(); // This will set autoStartNextSession to true (default)
    m_settingsChanged = true;
    Logger::Info("Pomodoro settings reset to defaults");
}