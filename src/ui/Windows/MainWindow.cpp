// MainWindow.cpp - Updated with Pomodoro Integration
#include "MainWindow.h"
#include "ui/Components/Sidebar.h"
#include "ui/Windows/PomodoroWindow.h"
#include "core/Timer/PomodoroTimer.h"
#include "core/Logger.h"
#include "core/Utils.h"
#include "app/Application.h"
#include "app/AppConfig.h"
#include <imgui.h>
#include <vector>
#include <algorithm>

#include "resource.h"

// Add this for OpenGL
#ifdef _WIN32
    #include <windows.h>
    #include <GL/gl.h>
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_LINEAR
#define GL_LINEAR 0x2601
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// File item structure for the dropover interface
struct FileItem
{
    std::string name;
    std::string fullPath;
    std::string size;
    std::string type;
    std::string modified;
    bool isDirectory;
    
    FileItem(const std::string& path) : fullPath(path)
    {
        name = Utils::GetFileName(path);
        isDirectory = Utils::DirectoryExists(path);
        
        if (!isDirectory)
        {
            uint64_t sizeBytes = Utils::GetFileSize(path);
            size = Utils::FormatBytes(sizeBytes);
            type = Utils::GetFileExtension(path);
        }
        else
        {
            size = "Folder";
            type = "Folder";
        }
        
        modified = "Recently"; // Simplified for now
    }
};

MainWindow::MainWindow()
{
    m_sidebar = std::make_unique<Sidebar>();
    m_alwaysOnTop = false;
}

MainWindow::~MainWindow()
{
    Shutdown();
    UnloadTexture(iconCut);
    UnloadTexture(iconPaste);
    UnloadTexture(iconTrash);
    UnloadTexture(iconRename);
    UnloadTexture(iconClear);
    UnloadTexture(iconPin);
    UnloadTexture(iconTrigger);
}

bool MainWindow::Initialize(AppConfig* config)
{
    m_config = config;
    
    // Initialize Pomodoro timer
    m_pomodoroTimer = std::make_unique<PomodoroTimer>();
    m_pomodoroSettingsWindow = std::make_unique<PomodoroWindow>();
    
    // Set up timer callbacks
    m_pomodoroTimer->SetOnSessionComplete([this](PomodoroTimer::SessionType type) {
        OnPomodoroSessionComplete(static_cast<int>(type));
    });
    
    m_pomodoroTimer->SetOnAllSessionsComplete([this]() {
        OnPomodoroAllComplete();
    });
    
    m_pomodoroTimer->SetOnTick([this]() {
        OnPomodoroTick();
    });
    
    // Initialize settings window (it manages its own config loading)
    m_pomodoroSettingsWindow = std::make_unique<PomodoroWindow>();
    if (!m_pomodoroSettingsWindow->Initialize(config))
    {
        Logger::Warning("Failed to initialize Pomodoro settings window");
    }
    
    // Load Pomodoro configuration
    if (config)
    {
        PomodoroTimer::PomodoroConfig pomodoroConfig;
        
        // Load from config
        pomodoroConfig.workDurationMinutes = config->GetValue("pomodoro.work_duration", 25);
        pomodoroConfig.shortBreakMinutes = config->GetValue("pomodoro.short_break", 5);
        pomodoroConfig.longBreakMinutes = config->GetValue("pomodoro.long_break", 15);
        pomodoroConfig.totalSessions = config->GetValue("pomodoro.total_sessions", 8);
        pomodoroConfig.sessionsBeforeLongBreak = config->GetValue("pomodoro.sessions_before_long_break", 4);
        pomodoroConfig.autoStartNextSession = config->GetValue("pomodoro.auto_start_next", true); // Load auto-start setting
        
        // Load colors
        pomodoroConfig.colorHigh.r = config->GetValue("pomodoro.color_high_r", 0.0f);
        pomodoroConfig.colorHigh.g = config->GetValue("pomodoro.color_high_g", 1.0f);
        pomodoroConfig.colorHigh.b = config->GetValue("pomodoro.color_high_b", 0.0f);
        
        pomodoroConfig.colorMedium.r = config->GetValue("pomodoro.color_medium_r", 1.0f);
        pomodoroConfig.colorMedium.g = config->GetValue("pomodoro.color_medium_g", 1.0f);
        pomodoroConfig.colorMedium.b = config->GetValue("pomodoro.color_medium_b", 0.0f);
        
        pomodoroConfig.colorLow.r = config->GetValue("pomodoro.color_low_r", 1.0f);
        pomodoroConfig.colorLow.g = config->GetValue("pomodoro.color_low_g", 0.5f);
        pomodoroConfig.colorLow.b = config->GetValue("pomodoro.color_low_b", 0.0f);
        
        pomodoroConfig.colorCritical.r = config->GetValue("pomodoro.color_critical_r", 1.0f);
        pomodoroConfig.colorCritical.g = config->GetValue("pomodoro.color_critical_g", 0.0f);
        pomodoroConfig.colorCritical.b = config->GetValue("pomodoro.color_critical_b", 0.0f);
        
        m_pomodoroTimer->SetConfig(pomodoroConfig);
    }
    
    Logger::Info("MainWindow initialized successfully");
    return true;
}

void MainWindow::Shutdown()
{
    if (m_pomodoroSettingsWindow)
        m_pomodoroSettingsWindow->Shutdown();
    
    m_pomodoroSettingsWindow.reset();
    m_pomodoroTimer.reset();
}

void MainWindow::Update(float deltaTime)
{
    if (m_sidebar)
    {
        m_sidebar->Update(deltaTime);
        
        // Check if sidebar navigation changed
        ModulePage newModule = m_sidebar->GetSelectedModule();
        if (newModule != m_currentModule)
        {
            SetCurrentModule(newModule);
        }
    }
    
    // Update Pomodoro timer
    if (m_pomodoroTimer)
    {
        m_pomodoroTimer->Update();
    }
    
    // Update settings window if visible
    if (m_pomodoroSettingsWindow && m_showPomodoroSettings)
    {
        m_pomodoroSettingsWindow->Update(deltaTime);
    }
}

void MainWindow::Render()
{
    // Get display size for fullscreen window
    ImGuiIO& io = ImGui::GetIO();
    
    // Set window to cover entire display
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    
    // Main window flags
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | 
                                  ImGuiWindowFlags_NoMove | 
                                  ImGuiWindowFlags_NoResize | 
                                  ImGuiWindowFlags_NoSavedSettings |
                                  ImGuiWindowFlags_NoBringToFrontOnFocus |
                                  ImGuiWindowFlags_MenuBar; // Enable menubar
    
    // Begin main window
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##MainWindow", nullptr, windowFlags);
    ImGui::PopStyleVar();
    
    // Render menubar
    RenderMenuBar();
    
    // Sidebar
    if (m_sidebar)
    {
        m_sidebar->Render();
    }
    
    // Content area
    RenderContentArea();
    
    ImGui::End();
    
    // Render Pomodoro settings window if needed
    if (m_pomodoroSettingsWindow && m_showPomodoroSettings)
    {
        m_pomodoroSettingsWindow->SetVisible(true);
        m_pomodoroSettingsWindow->Render();
        
        // Check if settings window was closed
        if (!m_pomodoroSettingsWindow->IsVisible())
        {
            m_showPomodoroSettings = false;
        }
    }
}

void MainWindow::SetCurrentModule(ModulePage module)
{
    m_currentModule = module;
    Logger::Debug("Switched to module: {}", GetModuleName(module));
}

void MainWindow::RenderContentArea()
{
    // Calculate content area (main area minus sidebar)
    float sidebarWidth = m_sidebar ? m_sidebar->GetWidth() : 50.0f;
    float menuBarHeight = ImGui::GetFrameHeight(); // Account for menubar
    
    ImVec2 contentPos = ImVec2(sidebarWidth, menuBarHeight);
    ImVec2 contentSize = ImVec2(ImGui::GetContentRegionAvail().x, 
                               ImGui::GetContentRegionAvail().y);
    
    // Set cursor position for content area
    ImGui::SetCursorPos(contentPos);
    
    // Begin content area
    ImGui::BeginChild("##ContentArea", contentSize, false, ImGuiWindowFlags_NoScrollbar);
    
    // Render current module content
    switch (m_currentModule)
    {
        case ModulePage::Dashboard:
            RenderDropoverInterface();
            break;
        case ModulePage::Pomodoro:
            RenderPomodoroModule(); // Updated to actual implementation
            break;
        case ModulePage::Kanban:
            RenderKanbanPlaceholder();
            break;
        case ModulePage::Clipboard:
            RenderClipboardPlaceholder();
            break;
        case ModulePage::Todo:
            RenderBulkRenamePlaceholder(); // Todo: change to todo
            break;
        case ModulePage::FileConverter:
            RenderFileConverterPlaceholder();
            break;
        case ModulePage::Settings:
            renderSettingPlaceholder();
            break;
        default:
            ImGui::Text("Unknown module");
            break;
    }
    
    ImGui::EndChild();
}

// Pomodoro Module Implementation
void MainWindow::RenderPomodoroModule()
{
    if (!m_pomodoroTimer)
        return;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
    
    // Header with title and settings button
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use default font, could be larger
    ImGui::Text("🍅 Pomodoro Timer");
    ImGui::PopFont();
    
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - 100);
    if (ImGui::Button("⚙ Settings"))
    {
        m_showPomodoroSettings = true;
    }
    
    ImGui::Separator();
    ImGui::Spacing();
    
    // Main timer display
    RenderPomodoroTimer();
    
    ImGui::Spacing();
    
    // Progress bar
    RenderPomodoroProgress();
    
    ImGui::Spacing();
    
    // Session information
    RenderPomodoroSessionInfo();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Control buttons
    RenderPomodoroControls();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Quick settings
    RenderPomodoroQuickSettings();
    
    ImGui::PopStyleVar();
}

void MainWindow::RenderPomodoroTimer()
{
    auto color = m_pomodoroTimer->GetCurrentColor();
    auto state = m_pomodoroTimer->GetState();
    
    // Large timer text - centered
    std::string timeText = m_pomodoroTimer->GetFormattedTimeRemaining();
    
    // Add "PAUSED" indicator when paused
    std::string displayText = timeText;
    if (state == PomodoroTimer::TimerState::Paused)
    {
        displayText = timeText + " (PAUSED)";
    }
    
    // Create a more prominent timer display using a bordered approach
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    float centerX = availableSize.x * 0.5f;
    
    // Calculate text size for centering (use display text for proper sizing)
    ImVec2 textSize = ImGui::CalcTextSize(displayText.c_str());
    
    // Create a background box for the timer
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 cursorPos = ImGui::GetCursorPos();
    
    // Timer background box
    float boxWidth = textSize.x + 60.0f;
    float boxHeight = textSize.y + 30.0f;
    ImVec2 boxMin = ImVec2(windowPos.x + cursorPos.x + centerX - boxWidth * 0.5f, 
                          windowPos.y + cursorPos.y);
    ImVec2 boxMax = ImVec2(boxMin.x + boxWidth, boxMin.y + boxHeight);
    
    // Draw background with color-coded border
    ImU32 borderColor = IM_COL32(
        static_cast<int>(color.r * 255), 
        static_cast<int>(color.g * 255), 
        static_cast<int>(color.b * 255), 
        255
    );
    
    // Different background for paused state
    ImU32 bgColor;
    if (state == PomodoroTimer::TimerState::Paused)
    {
        bgColor = IM_COL32(40, 40, 40, 200); // Darker background when paused
    }
    else
    {
        bgColor = IM_COL32(20, 20, 20, 200); // Normal background
    }
    
    drawList->AddRectFilled(boxMin, boxMax, bgColor, 8.0f);
    drawList->AddRect(boxMin, boxMax, borderColor, 8.0f, 0, 3.0f);
    
    // Position cursor for text
    ImGui::SetCursorPosX(centerX - textSize.x * 0.5f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 15.0f);
    
    // Render text in a bright white/colored style
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(color.r, color.g, color.b, 1.0f));
    
    // Use the display text (which includes PAUSED indicator if needed)
    ImGui::Text("%s", displayText.c_str());
    
    ImGui::PopStyleColor();
    
    // Move cursor past the timer box
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 15.0f);
}

void MainWindow::RenderPomodoroProgress()
{
    float progress = 1.0f - m_pomodoroTimer->GetProgressPercentage(); // Invert for visual progress
    auto color = m_pomodoroTimer->GetCurrentColor();
    
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(color.r, color.g, color.b, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 0.8f));
    
    ImGui::ProgressBar(progress, ImVec2(-1.0f, 30.0f), "");
    
    ImGui::PopStyleColor(2);
}

void MainWindow::RenderPomodoroSessionInfo()
{
    auto sessionInfo = m_pomodoroTimer->GetCurrentSession();
    
    // Session description
    ImGui::Text("Current: %s", m_pomodoroTimer->GetSessionDescription().c_str());
    
    // Progress
    ImGui::Text("Completed Sessions: %d/%d", 
               m_pomodoroTimer->GetCompletedSessions(), 
               sessionInfo.totalSessions);
    
    // Pause time if any
    if (sessionInfo.pausedTime.count() > 0)
    {
        int pausedMinutes = static_cast<int>(sessionInfo.pausedTime.count() / 60);
        int pausedSeconds = static_cast<int>(sessionInfo.pausedTime.count() % 60);
        ImGui::Text("Paused time this session: %02d:%02d", pausedMinutes, pausedSeconds);
    }
    
    // Total pause time
    auto totalPaused = m_pomodoroTimer->GetTotalPausedTime();
    if (totalPaused.count() > 0)
    {
        int totalPausedMinutes = static_cast<int>(totalPaused.count() / 60);
        int totalPausedSeconds = static_cast<int>(totalPaused.count() % 60);
        ImGui::Text("Total paused time: %02d:%02d", totalPausedMinutes, totalPausedSeconds);
    }
}

void MainWindow::RenderPomodoroControls()
{
    float buttonWidth = 100.0f;
    float spacing = 15.0f;
    float totalWidth = 4 * buttonWidth + 3 * spacing;
    float windowWidth = ImGui::GetContentRegionAvail().x;
    
    ImGui::SetCursorPosX((windowWidth - totalWidth) * 0.5f);
    
    auto state = m_pomodoroTimer->GetState();
    
    // Start/Pause/Resume button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    
    if (state == PomodoroTimer::TimerState::Stopped)
    {
        if (ImGui::Button("▶ Start", ImVec2(buttonWidth, 40)))
            m_pomodoroTimer->Start();
    }
    else if (state == PomodoroTimer::TimerState::Running)
    {
        ImGui::PopStyleColor(2);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.7f, 0.2f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.8f, 0.3f, 1.0f));
        
        if (ImGui::Button("⏸ Pause", ImVec2(buttonWidth, 40)))
            m_pomodoroTimer->Pause();
    }
    else if (state == PomodoroTimer::TimerState::Paused)
    {
        if (ImGui::Button("▶ Resume", ImVec2(buttonWidth, 40)))
            m_pomodoroTimer->Resume();
    }
    
    ImGui::PopStyleColor(2);
    
    ImGui::SameLine(0, spacing);
    
    // Stop button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    
    if (ImGui::Button("⏹ Stop", ImVec2(buttonWidth, 40)))
        m_pomodoroTimer->Stop();
    
    ImGui::PopStyleColor(2);
    
    ImGui::SameLine(0, spacing);
    
    // Skip button
    bool canSkip = (state == PomodoroTimer::TimerState::Running || 
                   state == PomodoroTimer::TimerState::Paused);
    
    if (!canSkip) ImGui::BeginDisabled();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.7f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.8f, 1.0f));
    
    if (ImGui::Button("⏭ Skip", ImVec2(buttonWidth, 40)))
        m_pomodoroTimer->Skip();
    
    ImGui::PopStyleColor(2);
    
    if (!canSkip) ImGui::EndDisabled();
    
    ImGui::SameLine(0, spacing);
    
    // Reset button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    
    if (ImGui::Button("🔄 Reset", ImVec2(buttonWidth, 40)))
        m_pomodoroTimer->Reset();
    
    ImGui::PopStyleColor(2);
}

void MainWindow::RenderPomodoroQuickSettings()
{
    if (ImGui::CollapsingHeader("Quick Settings"))
    {
        auto config = m_pomodoroTimer->GetConfig();
        bool configChanged = false;
        
        ImGui::Columns(2, "QuickSettings", false);
        ImGui::SetColumnWidth(0, 200.0f);
        
        // Work duration
        ImGui::Text("Work Duration:");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(-1);
        int workDuration = config.workDurationMinutes;
        if (ImGui::SliderInt("##work", &workDuration, 1, 60, "%d min"))
        {
            config.workDurationMinutes = workDuration;
            configChanged = true;
        }
        ImGui::NextColumn();
        
        // Short break
        ImGui::Text("Short Break:");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(-1);
        int shortBreak = config.shortBreakMinutes;
        if (ImGui::SliderInt("##short", &shortBreak, 1, 30, "%d min"))
        {
            config.shortBreakMinutes = shortBreak;
            configChanged = true;
        }
        ImGui::NextColumn();
        
        // Long break
        ImGui::Text("Long Break:");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(-1);
        int longBreak = config.longBreakMinutes;
        if (ImGui::SliderInt("##long", &longBreak, 5, 60, "%d min"))
        {
            config.longBreakMinutes = longBreak;
            configChanged = true;
        }
        ImGui::NextColumn();
        
        // Total sessions
        ImGui::Text("Total Sessions:");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(-1);
        int totalSessions = config.totalSessions;
        if (ImGui::SliderInt("##total", &totalSessions, 1, 20))
        {
            config.totalSessions = totalSessions;
            configChanged = true;
        }
        ImGui::NextColumn();
        
        // Auto-start next session toggle
        ImGui::Text("Auto-start Next:");
        ImGui::NextColumn();
        bool autoStart = config.autoStartNextSession;
        if (ImGui::Checkbox("##autostart", &autoStart))
        {
            config.autoStartNextSession = autoStart;
            configChanged = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Automatically start the next session when current one ends");
        }
        
        ImGui::Columns(1);
        
        if (configChanged)
        {
            m_pomodoroTimer->SetConfig(config);
            
            // Save to config file
            if (m_config)
            {
                m_config->SetValue("pomodoro.work_duration", config.workDurationMinutes);
                m_config->SetValue("pomodoro.short_break", config.shortBreakMinutes);
                m_config->SetValue("pomodoro.long_break", config.longBreakMinutes);
                m_config->SetValue("pomodoro.total_sessions", config.totalSessions);
                m_config->SetValue("pomodoro.auto_start_next", config.autoStartNextSession);
                m_config->Save();
            }
        }
    }
}

// Pomodoro Event Handlers
void MainWindow::OnPomodoroSessionComplete(int sessionType)
{
    std::string message;
    
    // Use simple integer comparison to avoid casting issues
    if (sessionType == 0) // Work session
    {
        message = "Work session completed! Time for a break.";
    }
    else if (sessionType == 1) // Short break
    {
        message = "Short break finished! Back to work.";
    }
    else if (sessionType == 2) // Long break
    {
        message = "Long break finished! Ready for more work.";
    }
    else
    {
        message = "Session completed!";
    }
    
    Logger::Info("Pomodoro: {}", message);
    // TODO: Show system notification when available
}

void MainWindow::OnPomodoroAllComplete()
{
    Logger::Info("Pomodoro: All sessions completed! Excellent work!");
    // TODO: Show completion notification when available
}

void MainWindow::OnPomodoroTick()
{
    // Could be used for animations or updates
}

// [Keep all the existing methods for texture loading, dropover interface, placeholders, etc...]
// [This is just the updated/new code - the rest remains the same]

ImTextureID MainWindow::LoadTextureFromFile(const char* filename, int* out_width, int* out_height)
{
    // Load image data from file
    int image_width = 0, image_height = 0;
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
    if (!image_data)
        return nullptr;

    // Create OpenGL texture
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);

    if (out_width)  *out_width = image_width;
    if (out_height) *out_height = image_height;

    return (ImTextureID)(intptr_t)image_texture;
}

void MainWindow::UnloadTexture(ImTextureID tex_id)
{
    GLuint tex = (GLuint)(intptr_t)tex_id;
    glDeleteTextures(1, &tex);
}

unsigned char* MainWindow::LoadPNGFromResource(int resourceID, int* out_width, int* out_height)
{
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(resourceID), RT_RCDATA);
    if (!hRes) return nullptr;

    HGLOBAL hMem = LoadResource(NULL, hRes);
    if (!hMem) return nullptr;

    DWORD size = SizeofResource(NULL, hRes);
    void* data = LockResource(hMem);

    int width, height, channels;
    unsigned char* image_data = stbi_load_from_memory(
        reinterpret_cast<unsigned char*>(data),
        static_cast<int>(size),
        &width, &height, &channels,
        4 // force RGBA
    );

    if (!image_data) return nullptr;

    if (out_width)  *out_width = width;
    if (out_height) *out_height = height;

    return image_data;
}

ImTextureID MainWindow::LoadTextureFromResource(int resourceID)
{
    int width = 0, height = 0;
    unsigned char* image_data = LoadPNGFromResource(resourceID, &width, &height);
    if (!image_data)
        return nullptr;

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image_data);

    stbi_image_free(image_data);

    return (ImTextureID)(intptr_t)texture;
}

void MainWindow::RenderMenuBar()
{
    if (ImGui::BeginMenuBar())
    {
        // File Menu
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Session", "Ctrl+N"))
            {
                Logger::Info("New Session clicked");
                // TODO: Implement new session
            }
            
            if (ImGui::MenuItem("Open...", "Ctrl+O"))
            {
                Logger::Info("Open clicked");
                // TODO: Implement file open dialog
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Exit"))
            {
                Logger::Info("Exit clicked from menu");
                if (Application::GetInstance())
                {
                    Application::GetInstance()->RequestExit();
                }
            }
            
            ImGui::EndMenu();
        }
        
        // Window Menu
        if (ImGui::BeginMenu("Window"))
        {
            if (ImGui::MenuItem("Always on Top", nullptr, m_alwaysOnTop))
            {
                m_alwaysOnTop = !m_alwaysOnTop;
                Logger::Info("Always on Top toggled: {}", m_alwaysOnTop);
                // TODO: Implement always on top functionality
            }
            
            if (ImGui::MenuItem("Minimize to Tray"))
            {
                Logger::Info("Minimize to Tray clicked");
                if (Application::GetInstance())
                {
                    Application::GetInstance()->HideMainWindow();
                }
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Reset Layout"))
            {
                Logger::Info("Reset Layout clicked");
                // TODO: Reset window layout
            }
            
            ImGui::EndMenu();
        }
        
        // Tools Menu
        if (ImGui::BeginMenu("Tools"))
        {
            if (ImGui::MenuItem("Settings", "F12"))
            {
                Logger::Info("Settings clicked from menu");
                // TODO: Open settings window
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Open Log Folder"))
            {
                Logger::Info("Open Log Folder clicked");
                // TODO: Open log folder in explorer
            }
            
            ImGui::EndMenu();
        }
        
        // Help Menu
        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("User Guide"))
            {
                Logger::Info("User Guide clicked");
                // TODO: Open user guide
            }
            
            if (ImGui::MenuItem("Keyboard Shortcuts"))
            {
                Logger::Info("Keyboard Shortcuts clicked");
                // TODO: Show shortcuts dialog
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("About Potensio"))
            {
                Logger::Info("About clicked from menu");
                // TODO: Show about dialog
            }
            
            ImGui::EndMenu();
        }
        
        ImGui::EndMenuBar();
    }
}

void MainWindow::RenderDropoverInterface()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    
    // Toolbar
    RenderDropoverToolbar();
    ImGui::Spacing();
    
    // Main dropover area
    RenderDropoverArea();
    
    ImGui::PopStyleVar();
}

void MainWindow::RenderDropoverToolbar()
{
    // File operations toolbar with icons
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
    
    if (ImGui::ImageButton(iconPaste, ImVec2(16, 16)))
    {
        Logger::Info("Paste Here clicked");
        // TODO: Implement paste from clipboard
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Paste Here");
    
    ImGui::SameLine();
    if (ImGui::ImageButton(iconCut, ImVec2(16, 16)))
    {
        Logger::Info("Cut Files clicked");
        // TODO: Implement cut selected files
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cut Files");

    ImGui::SameLine();
    if (ImGui::ImageButton(iconRename, ImVec2(16, 16)))
    {
        Logger::Info("Bulk Rename clicked");
        // TODO: Open bulk rename dialog
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bulk Rename");
    
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    if (ImGui::ImageButton(iconClear, ImVec2(16, 16)))
    {
        Logger::Info("Clear All clicked");
        // TODO: Clear all files from dropover
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear All");
    
    ImGui::SameLine();
    if (ImGui::ImageButton(iconTrash, ImVec2(16, 16)))
    {
        Logger::Info("Recycle Bin clicked");
        // TODO: Implement delete selected files
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Recycle Bin");

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    // View options
    if (ImGui::ImageButton(iconPin, ImVec2(16, 16)))
    {
        Logger::Info("Pin Window is clicked");
        // TODO: Clear all files from dropover
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pin Window");

    ImGui::SameLine();
    if (ImGui::ImageButton(iconTrigger, ImVec2(16, 16)))
    {
        Logger::Info("Trigger toggled");
        // TODO: Clear all files from dropover
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle trigger");
    
    ImGui::PopStyleVar();
}

void MainWindow::RenderDropoverArea()
{
    // Create a drop zone
    ImVec2 dropZoneSize = ImVec2(ImGui::GetContentRegionAvail().x - 16, 150);
    ImVec2 dropZonePos = ImGui::GetCursorPos();
    
    // Drop zone background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 dropZoneMin = ImVec2(windowPos.x + dropZonePos.x + 8, windowPos.y + dropZonePos.y);
    ImVec2 dropZoneMax = ImVec2(dropZoneMin.x + dropZoneSize.x, dropZoneMin.y + dropZoneSize.y);
    
    // Draw drop zone border
    drawList->AddRect(dropZoneMin, dropZoneMax, IM_COL32(100, 100, 100, 255), 4.0f, 0, 2.0f);
    drawList->AddRectFilled(dropZoneMin, dropZoneMax, IM_COL32(25, 25, 25, 255), 4.0f);
    
    // Drop zone text with icon
    ImGui::SetCursorPos(ImVec2(dropZonePos.x + dropZoneSize.x * 0.5f - 100, dropZonePos.y + 50));
    ImGui::Text("Drop files here to stage them");
    ImGui::SetCursorPos(ImVec2(dropZonePos.x + dropZoneSize.x * 0.5f - 80, dropZonePos.y + 80));
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Drag and drop files or folders");
    
    // Move cursor past drop zone
    ImGui::SetCursorPos(ImVec2(dropZonePos.x, dropZonePos.y + dropZoneSize.y + 16));
    
    // File list header
    ImGui::Text("Staged Files:");
    ImGui::Separator();
    
    // File list area
    RenderFileList();
}

void MainWindow::RenderFileList()
{
    // Sample files for demonstration (in real implementation, this would be dynamic)
    static std::vector<FileItem> stagedFiles;
    
    // Add sample files for demonstration
    if (stagedFiles.empty())
    {
        // These would be actual files dropped by user
        stagedFiles.push_back(FileItem("C:\\Windows\\System32\\notepad.exe"));
        stagedFiles.push_back(FileItem("C:\\Windows\\System32"));
    }
    
    // Table for file list (like Windows Explorer detail view)
    if (ImGui::BeginTable("##FileListTable", 4, 
        ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | 
        ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg))
    {
        // Setup columns
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableHeadersRow();
        
        // Render file entries
        for (size_t i = 0; i < stagedFiles.size(); ++i)
        {
            const FileItem& file = stagedFiles[i];
            
            ImGui::TableNextRow();
            
            // Name column with file icon
            ImGui::TableNextColumn();
            bool isSelected = false; // TODO: Track selection
            
            // Add file type icon
            std::string displayName = (file.isDirectory ? "(D) " : "(F) ") + file.name;
            
            if (ImGui::Selectable(displayName.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
            {
                Logger::Debug("Selected file: {}", file.name);
                // TODO: Handle file selection
            }
            
            // Context menu for files
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Remove from staging"))
                {
                    stagedFiles.erase(stagedFiles.begin() + i);
                    ImGui::EndPopup();
                    break;
                }
                if (ImGui::MenuItem("Open"))
                {
                    Logger::Info("Opening file: {}", file.fullPath);
                    // TODO: Open file
                }
                if (ImGui::MenuItem("Show in Explorer"))
                {
                    Logger::Info("Showing in explorer: {}", file.fullPath);
                    // TODO: Show in explorer
                }
                ImGui::EndPopup();
            }
            
            // Size column
            ImGui::TableNextColumn();
            ImGui::Text("%s", file.size.c_str());
            
            // Type column
            ImGui::TableNextColumn();
            ImGui::Text("%s", file.type.c_str());
            
            // Modified column
            ImGui::TableNextColumn();
            ImGui::Text("%s", file.modified.c_str());
        }
        
        // Empty state
        if (stagedFiles.empty())
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No files staged");
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
        }
        
        ImGui::EndTable();
    }
    
    // Status bar
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Files staged: %zu", stagedFiles.size());
    if (!stagedFiles.empty())
    {
        ImGui::SameLine();
        ImGui::Text(" | Right-click for options");
    }
}

void MainWindow::InitializeResources()
{
    iconCut = LoadTextureFromResource(IDI_CUT_ICON);
    iconPaste   = LoadTextureFromResource(IDI_PASTE_ICON);
    iconTrash   = LoadTextureFromResource(IDI_REMOVE_ICON);
    iconRename   = LoadTextureFromResource(IDI_RENAME_ICON);
    iconClear   = LoadTextureFromResource(IDI_CLEAR_ICON);
    iconPin   = LoadTextureFromResource(IDI_PIN_ICON);
    iconTrigger   = LoadTextureFromResource(IDI_TRIGGER_ICON);
}

// Placeholder methods for other modules
void MainWindow::RenderKanbanPlaceholder()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    
    ImGui::Text("Kanban Board");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("The Kanban Board will be implemented in Sprint 3.");
    ImGui::Spacing();
    ImGui::Text("Features coming soon:");
    ImGui::BulletText("Multiple boards");
    ImGui::BulletText("Customizable columns");
    ImGui::BulletText("Drag-and-drop cards");
    ImGui::BulletText("Card descriptions and due dates");
    ImGui::BulletText("Progress tracking");
    
    ImGui::PopStyleVar();
}

void MainWindow::RenderClipboardPlaceholder()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    
    ImGui::Text("Clipboard Manager");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("The Clipboard Manager will be implemented in Sprint 3.");
    ImGui::Spacing();
    ImGui::Text("Features coming soon:");
    ImGui::BulletText("Clipboard history");
    ImGui::BulletText("Search and filter");
    ImGui::BulletText("Favorites");
    ImGui::BulletText("Auto-cleanup");
    ImGui::BulletText("Quick paste");
    
    ImGui::PopStyleVar();
}

void MainWindow::RenderBulkRenamePlaceholder()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    
    ImGui::Text("Bulk Rename");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("Bulk file renaming will be implemented in Sprint 4.");
    ImGui::Spacing();
    ImGui::Text("Features coming soon:");
    ImGui::BulletText("Pattern-based renaming");
    ImGui::BulletText("RegEx support");
    ImGui::BulletText("Case transformations");
    ImGui::BulletText("Number sequences");
    ImGui::BulletText("Preview before apply");
    
    ImGui::PopStyleVar();
}

void MainWindow::RenderFileConverterPlaceholder()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    
    ImGui::Text("File Converter");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("File conversion and compression will be implemented in Sprint 5.");
    ImGui::Spacing();
    ImGui::Text("Features coming soon:");
    ImGui::BulletText("PDF compression");
    ImGui::BulletText("Image format conversion");
    ImGui::BulletText("Image compression");
    ImGui::BulletText("Batch processing");
    ImGui::BulletText("Quality settings");
    
    ImGui::PopStyleVar();
}

void MainWindow::renderSettingPlaceholder()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    
    ImGui::Text("Settings");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("Settings in Sprint 6.");
    ImGui::Spacing();
    ImGui::Text("Features coming soon:");
    ImGui::BulletText("Theme");
    ImGui::BulletText("Account");
    ImGui::BulletText("Update");
    
    ImGui::PopStyleVar(); 
}

const char* MainWindow::GetModuleName(ModulePage module) const
{
    switch (module)
    {
        case ModulePage::Dashboard:     return "Dropover";
        case ModulePage::Pomodoro:      return "Pomodoro";
        case ModulePage::Kanban:        return "Kanban";
        case ModulePage::Clipboard:     return "Clipboard";
        case ModulePage::Todo:          return "Todo";
        case ModulePage::FileConverter: return "File Converter";
        case ModulePage::Settings:      return "Settings";
        default:                        return "Unknown";
    }
}