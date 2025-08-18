// MainWindow.cpp - Updated with Pomodoro Integration
#include "MainWindow.h"

#include "ui/Components/Sidebar.h"
#include "ui/Windows/PomodoroWindow.h"
#include "ui/Windows/KanbanWindow.h"
#include "ui/Windows/ClipboardWindow.h"

#include "core/Timer/PomodoroTimer.h"
#include "core/Todo/TodoManager.h"
#include "core/Database/DatabaseManager.h"
#include "core/Database/PomodoroDatabase.h"

#include "core/Utils.h"
#include "core/Logger.h"

#include "app/Application.h"
#include "app/AppConfig.h"

#include <windows.h>
#include <ShlObj.h>
#include <string>
#include <imgui.h>
#include <vector>
#include <algorithm>
#include <filesystem>

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

    // Pomodoro icon
    UnloadTexture(iconPlay);
    UnloadTexture(iconPause);
    UnloadTexture(iconReset);
    UnloadTexture(iconSkip);
    UnloadTexture(iconStop);
}

bool MainWindow::Initialize(AppConfig* config)
{
    m_config = config;

    // Initialize database
    if (!InitializeDatabase())
    {
        Logger::Error("Failed to initialize database - continuing without database support");
    }
    
    // Initialize Pomodoro timer
    m_pomodoroTimer = std::make_unique<PomodoroTimer>();
    m_pomodoroSettingsWindow = std::make_unique<PomodoroWindow>();

    // Load Pomodoro configuration from database first, then fallback to AppConfig
    LoadPomodoroConfiguration();
    
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
    
    // Initialize Pomodoro settings window (it manages its own config loading)
    if (!m_pomodoroSettingsWindow->Initialize(config))
    {
        Logger::Warning("Failed to initialize Pomodoro settings window");
    }
    
    // Initialize Kanban manager and window
    m_kanbanManager = std::make_unique<KanbanManager>();
    m_kanbanSettingsWindow = std::make_unique<KanbanWindow>();
    
    // Initialize Kanban components
    if (!m_kanbanManager->Initialize(config))
    {
        Logger::Warning("Failed to initialize Kanban manager");
    }
    
    if (!m_kanbanSettingsWindow->Initialize(config))
    {
        Logger::Warning("Failed to initialize Kanban settings window");
    }
    
    // Set up Kanban callbacks
    m_kanbanManager->SetOnCardUpdated([this](std::shared_ptr<Kanban::Card> card) {
        OnKanbanCardUpdated(card);
    });
    
    m_kanbanManager->SetOnBoardChanged([this](Kanban::Board* board) {
        OnKanbanBoardChanged(board);
    });
    
    m_kanbanManager->SetOnProjectChanged([this](Kanban::Project* project) {
        OnKanbanProjectChanged(project);
    });

    // Initialize Todo manager
    m_todoManager = std::make_unique<TodoManager>();
    
    // Initialize Todo components
    if (!m_todoManager->Initialize(config))
    {
        Logger::Warning("Failed to initialize Todo manager");
    }

    // Set up Todo callbacks
    m_todoManager->SetOnTaskUpdated([this](std::shared_ptr<Todo::Task> task) {
        OnTodoTaskUpdated(task);
    });
    
    m_todoManager->SetOnTaskCompleted([this](std::shared_ptr<Todo::Task> task) {
        OnTodoTaskCompleted(task);
    });
    
    m_todoManager->SetOnDayChanged([this](const std::string& date) {
        OnTodoDayChanged(date);
    });
    
    // Connect settings window to manager
    m_kanbanSettingsWindow->SetKanbanManager(m_kanbanManager.get());
    
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

    /**
     * @note Clipboard Module
     */
    m_clipboardManager = std::make_unique<ClipboardManager>();
    m_clipboardSettingsWindow = std::make_unique<ClipboardWindow>();
    
    // Initialize Clipboard components
    if (!m_clipboardManager->Initialize(config))
    {
        Logger::Warning("Failed to initialize Clipboard manager");
    }

    if (!m_clipboardSettingsWindow->Initialize(config))
    {
        Logger::Warning("Failed to initialize Clipboard settings window");
    }

    // Set up Clipboard callbacks
    m_clipboardManager->SetOnItemAdded([this](std::shared_ptr<Clipboard::ClipboardItem> item) {
        OnClipboardItemAdded(item);
    });

    m_clipboardManager->SetOnItemDeleted([this](const std::string& id) {
        OnClipboardItemDeleted(id);
    });

    m_clipboardManager->SetOnHistoryCleared([this]() {
        OnClipboardHistoryCleared();
    });

    // Connect settings window to manager
    m_clipboardSettingsWindow->SetClipboardManager(m_clipboardManager.get());

    // Initialize File Converter
    m_fileConverter = std::make_unique<FileConverter>();
    
    if (!m_fileConverter->Initialize())
    {
        Logger::Warning("Failed to initialize File Converter");
    }
    
    // Set up File Converter callbacks
    m_fileConverter->SetProgressCallback([this](const std::string& jobId, float progress) {
        OnFileConverterJobProgress(jobId, progress);
    });
    
    m_fileConverter->SetCompletionCallback([this](const std::string& jobId, bool success, const std::string& error) {
        OnFileConverterJobComplete(jobId, success, error);
    });
    
    // Load File Converter settings
    if (config)
    {
        m_fileConverterUIState.imageQuality = config->GetValue("file_converter.image_quality", 85);
        m_fileConverterUIState.pngCompression = config->GetValue("file_converter.png_compression", 6);
        m_fileConverterUIState.preserveMetadata = config->GetValue("file_converter.preserve_metadata", false);
        m_fileConverterUIState.targetSizeKB = config->GetValue("file_converter.target_size_kb", 0);
        m_fileConverterUIState.outputDirectory = config->GetValue("file_converter.output_directory", "");
        m_fileConverterUIState.useSourceDirectory = config->GetValue("file_converter.use_source_directory", true);
        m_fileConverterUIState.autoProcessJobs = config->GetValue("file_converter.auto_process", true);
    }

    // Initialize Settings UI State
    LoadSettingsFromConfig();

    /**
     * @note Log successfull initialization
     */
    Logger::Info("MainWindow initialized successfully");
    return true;
}

void MainWindow::Shutdown()
{
    if (m_pomodoroSettingsWindow)
        m_pomodoroSettingsWindow->Shutdown();
    
    if (m_kanbanSettingsWindow)
        m_kanbanSettingsWindow->Shutdown();
    
    if (m_kanbanManager)
        m_kanbanManager->Shutdown();

    if (m_todoManager)
        m_todoManager->Shutdown();    

    if (m_clipboardSettingsWindow)
        m_clipboardSettingsWindow->Shutdown();

    if (m_clipboardManager)
        m_clipboardManager->Shutdown();
    
    if (m_fileConverter)
        m_fileConverter->Shutdown();
    
    m_pomodoroSettingsWindow.reset();
    m_pomodoroTimer.reset();
    m_kanbanSettingsWindow.reset();
    m_kanbanManager.reset();
    m_todoManager.reset();
    m_clipboardSettingsWindow.reset();
    m_clipboardManager.reset();
    m_fileConverter.reset();

    // End any active session
    if (m_currentSessionId != -1 && m_pomodoroDatabase)
    {
        auto sessionInfo = m_pomodoroTimer->GetCurrentSession();
        int pausedSeconds = static_cast<int>(sessionInfo.pausedTime.count());
        
        m_pomodoroDatabase->EndSession(m_currentSessionId, false, pausedSeconds); // Not completed
        m_currentSessionId = -1;
    }

    // Shutdown database
    if (m_databaseManager)
    {
        m_databaseManager->Shutdown();
    }
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
    
    // Update settings windows if visible
    if (m_pomodoroSettingsWindow && m_showPomodoroSettings)
    {
        m_pomodoroSettingsWindow->Update(deltaTime);
    }
    
    if (m_kanbanSettingsWindow && m_showKanbanSettings)
    {
        m_kanbanSettingsWindow->Update(deltaTime);
    }

    // Update settings windows if visible
    if (m_clipboardSettingsWindow && m_showClipboardSettings)
    {
        m_clipboardSettingsWindow->Update(deltaTime);
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
    
    // Render Kanban settings window if needed
    if (m_kanbanSettingsWindow && m_showKanbanSettings)
    {
        m_kanbanSettingsWindow->SetVisible(true);
        m_kanbanSettingsWindow->Render();
        
        // Check if settings window was closed
        if (!m_kanbanSettingsWindow->IsVisible())
        {
            m_showKanbanSettings = false;
        }
    }

    if (m_clipboardSettingsWindow && m_showClipboardSettings)
    {
        m_clipboardSettingsWindow->SetVisible(true);
        m_clipboardSettingsWindow->Render();
        
        // Check if settings window was closed
        if (!m_clipboardSettingsWindow->IsVisible())
        {
            m_showClipboardSettings = false;
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
            RenderKanbanModule();
            break;
        case ModulePage::Clipboard:
            RenderClipboardModule();
            break;
        case ModulePage::Todo:
            RenderTodoModule();
            break;
        case ModulePage::FileConverter:
            RenderFileConverterModule();
            break;
        case ModulePage::Settings:
            RenderSettingsModule();
            break;
        default:
            ImGui::Text("Unknown module");
            break;
    }
    
    ImGui::EndChild();
}

// =============================================================================
// KANBAN MODULE IMPLEMENTATION
// =============================================================================

void MainWindow::RenderKanbanModule()
{
    if (!m_kanbanManager)
    {
        ImGui::Text("Kanban Manager not available");
        return;
    }
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    
    // Header
    RenderKanbanHeader();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Main board area
    RenderKanbanBoard();
    
    // Card edit dialog (modal)
    if (m_cardEditState.isEditing)
    {
        RenderCardEditDialog();
    }
    
    ImGui::PopStyleVar();
}

void MainWindow::RenderKanbanHeader()
{
    auto currentProject = m_kanbanManager->GetCurrentProject();
    auto currentBoard = m_kanbanManager->GetCurrentBoard();
    
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float buttonAreaWidth = 280.0f; // Reserve space for buttons
    float infoAreaWidth = availableWidth - buttonAreaWidth - 20.0f; // 20px for spacing
    
    // Start horizontal layout
    ImGui::BeginGroup();
    
    // Info area with horizontal scrolling if needed
    ImGui::BeginChild("##KanbanHeaderInfo", ImVec2(infoAreaWidth, 30.0f), false, 
                     ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    // All information on ONE line with separators
    bool needsSeparator = false;
    
    // Project info
    ImGui::Text("Project:");
    ImGui::SameLine();
    if (currentProject)
    {
        ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%s", currentProject->name.c_str());
    }
    else
    {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No project");
    }
    needsSeparator = true;
    
    // Board info
    if (needsSeparator) { ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine(); }
    ImGui::Text("Board:");
    ImGui::SameLine();
    if (currentBoard)
    {
        ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%s", currentBoard->name.c_str());
    }
    else
    {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No board");
    }
    
    // Board statistics (only if board exists)
    if (currentBoard)
    {
        // Cards count
        ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
        ImGui::Text("Cards:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%d", currentBoard->GetTotalCardCount());
        
        // Completed count
        ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
        ImGui::Text("Completed:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "%d", currentBoard->GetCompletedCardCount());
        
        // Columns count
        ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
        ImGui::Text("Columns:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%zu", currentBoard->columns.size());
        
        // Progress percentage
        int totalCards = currentBoard->GetTotalCardCount();
        int completedCards = currentBoard->GetCompletedCardCount();
        if (totalCards > 0)
        {
            float completionRate = (static_cast<float>(completedCards) / totalCards) * 100.0f;
            
            ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
            ImGui::Text("Progress:");
            ImGui::SameLine();
            
            // Color code the percentage
            ImVec4 progressColor;
            if (completionRate >= 80.0f)
                progressColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // Green
            else if (completionRate >= 50.0f)
                progressColor = ImVec4(0.8f, 0.8f, 0.2f, 1.0f); // Yellow
            else
                progressColor = ImVec4(0.8f, 0.4f, 0.2f, 1.0f); // Orange
            
            ImGui::TextColored(progressColor, "%.1f%%", completionRate);
        }
    }
    else if (currentProject)
    {
        // Project-level stats when no board selected
        ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
        ImGui::Text("Boards:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%d", currentProject->GetTotalBoardCount());
        
        ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
        ImGui::Text("Total Cards:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%d", currentProject->GetTotalCardCount());
    }
    
    ImGui::EndChild(); // End scrollable info area
    
    // Action buttons on the same line, fixed position on the right
    ImGui::SameLine();
    
    float buttonSpacing = 8.0f;
    float buttonWidth = 85.0f;
    
    // Action buttons with consistent sizing
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    
    // New Project button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));
    
    if (ImGui::Button("+ Project", ImVec2(buttonWidth, 28.0f)))
    {
        // Quick create project
        static int projectCounter = 1;
        std::string projectName = "Project " + std::to_string(projectCounter++);
        m_kanbanManager->CreateProject(projectName);
    }
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine(0, buttonSpacing);
    
    // New Board button (disabled if no project)
    bool hasProject = currentProject != nullptr;
    if (!hasProject) ImGui::BeginDisabled();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.3f, 0.7f, 1.0f));
    
    if (ImGui::Button("+ Board", ImVec2(buttonWidth, 28.0f)))
    {
        // Quick create board
        static int boardCounter = 1;
        std::string boardName = "Board " + std::to_string(boardCounter++);
        m_kanbanManager->CreateBoard(boardName);
    }
    ImGui::PopStyleColor(3);
    
    if (!hasProject) ImGui::EndDisabled();
    
    ImGui::SameLine(0, buttonSpacing);
    
    // Settings button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    
    if (ImGui::Button("Settings", ImVec2(buttonWidth, 28.0f)))
    {
        m_showKanbanSettings = true;
    }
    ImGui::PopStyleColor(3);
    
    ImGui::PopStyleVar(); // FrameRounding
    
    ImGui::EndGroup();
}

void MainWindow::RenderKanbanBoard()
{
    auto currentBoard = m_kanbanManager->GetCurrentBoard();
    if (!currentBoard)
    {
        ImVec2 centerPos = ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 100, 100);
        ImGui::SetCursorPos(centerPos);
        ImGui::Text("No board selected");
        ImGui::SetCursorPos(ImVec2(centerPos.x - 20, centerPos.y + 20));
        ImGui::Text("Create a project and board to get started.");
        return;
    }
    
    if (currentBoard->columns.empty())
    {
        ImVec2 centerPos = ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 100, 100);
        ImGui::SetCursorPos(centerPos);
        ImGui::Text("No columns in this board");
        ImGui::SetCursorPos(ImVec2(centerPos.x - 20, centerPos.y + 20));
        ImGui::Text("Add columns in the settings to get started.");
        return;
    }
    
    // Calculate fully responsive column dimensions
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float availableHeight = ImGui::GetContentRegionAvail().y;
    float columnSpacing = 8.0f; // Reduced spacing for better fit
    size_t columnCount = currentBoard->columns.size();
    
    // Always calculate column width to fit ALL columns in available space
    // No minimum width - columns will shrink to fit the window
    float totalSpacing = (columnCount - 1) * columnSpacing + 16.0f; // Include margins
    float columnWidth = (availableWidth - totalSpacing) / columnCount;
    
    // Ensure we don't get negative or zero width
    if (columnWidth < 50.0f) columnWidth = 50.0f;
    
    // Calculate column height to fill available vertical space
    float columnHeight = availableHeight - 16.0f; // Leave some margin
    
    // Always use no-scroll container - columns must fit in window
    ImGui::BeginChild("##KanbanScrollArea", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(8, 8));
    
    // Render columns side by side - all will fit in window
    for (size_t i = 0; i < currentBoard->columns.size(); ++i)
    {
        if (i > 0)
        {
            ImGui::SameLine(0, columnSpacing);
        }
        
        ImGui::BeginGroup();
        RenderKanbanColumn(currentBoard->columns[i].get(), static_cast<int>(i), columnWidth, columnHeight);
        ImGui::EndGroup();
    }
    
    ImGui::EndChild();
}

void MainWindow::RenderKanbanColumn(Kanban::Column* column, int columnIndex, float columnWidth, float columnHeight)
{
    if (!column) return;
    
    float headerHeight = 60.0f;
    float padding = 6.0f; // Reduced padding for narrow columns
    
    // Adjust header height for very narrow columns
    if (columnWidth < 150.0f)
    {
        headerHeight = 75.0f; // More height needed for wrapped text
        padding = 4.0f;
    }
    
    // Column background and border
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 cursorPos = ImGui::GetCursorPos();
    
    ImVec2 columnMin = ImVec2(windowPos.x + cursorPos.x, windowPos.y + cursorPos.y);
    ImVec2 columnMax = ImVec2(columnMin.x + columnWidth, columnMin.y + columnHeight);
    
    // Column background with subtle gradient
    ImU32 columnBgColor = IM_COL32(40, 42, 46, 255);
    ImU32 columnBorderColor = IM_COL32(70, 75, 82, 255);
    
    drawList->AddRectFilled(columnMin, columnMax, columnBgColor, 6.0f);
    drawList->AddRect(columnMin, columnMax, columnBorderColor, 6.0f, 0, 1.5f);
    
    // Begin column content with proper positioning
    ImGui::SetCursorPos(cursorPos);
    ImGui::BeginChild(("##Column" + column->id).c_str(), ImVec2(columnWidth, columnHeight), false, ImGuiWindowFlags_NoScrollbar);
    
    // Column header area with text wrapping for narrow columns
    ImGui::SetCursorPos(ImVec2(padding, padding));
    
    // Column title with wrapping for narrow columns
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    if (columnWidth < 120.0f)
    {
        // For very narrow columns, wrap text
        ImGui::PushTextWrapPos(columnWidth - 2 * padding);
        ImGui::Text("%s", column->name.c_str());
        ImGui::PopTextWrapPos();
    }
    else
    {
        ImGui::Text("%s", column->name.c_str());
    }
    ImGui::PopStyleColor();
    
    // Card count - adjust format based on width
    float countYPos = columnWidth < 120.0f ? ImGui::GetCursorPosY() + 2 : padding + 18;
    ImGui::SetCursorPos(ImVec2(padding, countYPos));
    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    if (column->cardLimit > 0)
    {
        if (static_cast<int>(column->cards.size()) >= column->cardLimit)
        {
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        }
        
        // Shorter format for narrow columns
        if (columnWidth < 100.0f)
        {
            ImGui::Text("(%zu/%d)", column->cards.size(), column->cardLimit);
        }
        else
        {
            ImGui::Text("(%zu/%d cards)", column->cards.size(), column->cardLimit);
        }
    }
    else
    {
        if (columnWidth < 100.0f)
        {
            ImGui::Text("(%zu)", column->cards.size());
        }
        else
        {
            ImGui::Text("(%zu cards)", column->cards.size());
        }
    }
    ImGui::PopStyleColor();
    
    // Quick add card button - position adjusted for narrow columns
    float buttonYPos = columnWidth < 120.0f ? ImGui::GetCursorPosY() + 4 : padding + 35;
    ImGui::SetCursorPos(ImVec2(padding, buttonYPos));
    RenderQuickAddCard(column->id, columnWidth - 2 * padding);
    
    // Separator line
    ImGui::SetCursorPos(ImVec2(padding, headerHeight - 5));
    ImGui::Separator();
    
    // Cards area with proper sizing to fill remaining space
    ImVec2 cardsAreaPos = ImVec2(padding, headerHeight);
    ImVec2 cardsAreaSize = ImVec2(columnWidth - 2 * padding, columnHeight - headerHeight - padding);
    
    ImGui::SetCursorPos(cardsAreaPos);
    ImGui::BeginChild(("##CardsArea" + column->id).c_str(), cardsAreaSize, false, 
                     ImGuiWindowFlags_AlwaysVerticalScrollbar);
    
    // Render cards with proper spacing
    for (size_t cardIndex = 0; cardIndex < column->cards.size(); ++cardIndex)
    {
        auto card = column->cards[cardIndex];
        if (card)
        {
            if (cardIndex > 0)
            {
                ImGui::Spacing();
            }
            
            RenderKanbanCard(card, static_cast<int>(cardIndex), column->id);
        }
    }
    
    // Add some padding at the bottom for better UX
    ImGui::Spacing();
    ImGui::Spacing();
    
    // Drop target at the end of the column
    RenderDropTarget(column->id, static_cast<int>(column->cards.size()));
    
    ImGui::EndChild(); // End cards area
    ImGui::EndChild(); // End column
}

void MainWindow::RenderQuickAddCard(const std::string& columnId, float maxWidth)
{
    std::string inputId = "##quickadd" + columnId;
    
    static std::unordered_map<std::string, std::string> quickAddTexts;
    static std::unordered_map<std::string, bool> quickAddActive;
    
    bool isActive = quickAddActive[columnId];
    
    // Use provided max width or calculate from available space
    float buttonWidth = maxWidth > 0 ? maxWidth : (ImGui::GetContentRegionAvail().x - 16.0f);
    
    // Determine button text based on width
    std::string buttonText;
    if (buttonWidth < 80.0f)
    {
        buttonText = "+ Card##" + columnId;
    }
    else if (buttonWidth < 120.0f)
    {
        buttonText = "+ Add##" + columnId;
    }
    else
    {
        buttonText = "Add Card##" + columnId;
    }
    
    if (!isActive)
    {
        // Properly sized button that fits the column
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.5f, 0.8f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.4f, 0.7f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        
        if (ImGui::Button(buttonText.c_str(), ImVec2(buttonWidth, 24.0f)))
        {
            quickAddActive[columnId] = true;
            quickAddTexts[columnId] = "";
        }
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }
    else
    {
        // Quick add input with proper styling
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3)); // Smaller padding for narrow columns
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
        
        char buffer[256];
        strncpy_s(buffer, quickAddTexts[columnId].c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        
        ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(buttonWidth);
        bool enterPressed = ImGui::InputText(inputId.c_str(), buffer, sizeof(buffer), 
                                           ImGuiInputTextFlags_EnterReturnsTrue | 
                                           ImGuiInputTextFlags_AutoSelectAll);
        quickAddTexts[columnId] = buffer;
        
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        
        if (enterPressed && strlen(buffer) > 0)
        {
            // Create the card
            m_kanbanManager->CreateCard(columnId, buffer);
            quickAddActive[columnId] = false;
            quickAddTexts[columnId] = "";
        }
        
        // Cancel on escape or click outside
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || 
            (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered()))
        {
            quickAddActive[columnId] = false;
            quickAddTexts[columnId] = "";
        }
        
        // Small instruction text - only show if there's enough width
        if (buttonWidth > 100.0f)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1);
            ImGui::Text("Enter to add, Esc to cancel");
            ImGui::PopStyleColor();
        }
        else if (buttonWidth > 60.0f)
        {
            // Shorter instruction for narrow columns
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1);
            ImGui::Text("Enter/Esc");
            ImGui::PopStyleColor();
        }
        // No instruction text for very narrow columns
    }
}

void MainWindow::RenderKanbanCard(std::shared_ptr<Kanban::Card> card, int cardIndex, const std::string& columnId)
{
    if (!card) return;
    
    float cardWidth = ImGui::GetContentRegionAvail().x;
    float cardPadding = cardWidth < 100.0f ? 4.0f : 6.0f; // Smaller padding for narrow columns
    float minCardHeight = cardWidth < 100.0f ? 50.0f : 60.0f; // Smaller min height for narrow columns
    
    // Card background color based on priority with better contrast
    auto priorityColor = card->GetPriorityColor();
    ImU32 cardBgColor = IM_COL32(
        static_cast<int>(priorityColor.r * 120 + 35),
        static_cast<int>(priorityColor.g * 120 + 35),
        static_cast<int>(priorityColor.b * 120 + 35),
        255
    );
    
    ImU32 cardBorderColor = IM_COL32(
        static_cast<int>(priorityColor.r * 180 + 75),
        static_cast<int>(priorityColor.g * 180 + 75),
        static_cast<int>(priorityColor.b * 180 + 75),
        255
    );
    
    // Check if this card is being dragged
    auto& dragState = m_kanbanManager->GetDragDropState();
    bool isDragging = dragState.isDragging && dragState.draggedCard && 
                     dragState.draggedCard->id == card->id;
    
    if (isDragging)
    {
        cardBgColor = IM_COL32(80, 80, 80, 200);
        cardBorderColor = IM_COL32(120, 120, 120, 200);
    }
    
    // Calculate card content height dynamically
    float lineHeight = ImGui::GetTextLineHeight();
    float cardContentHeight = cardPadding * 2 + lineHeight; // Title
    
    // Add height for description if present
    if (!card->description.empty())
    {
        // Calculate wrapped text height for responsive truncation
        std::string displayDesc = card->description;
        size_t maxLength = cardWidth < 100.0f ? 30 : (cardWidth < 150.0f ? 60 : 100);
        
        if (displayDesc.length() > maxLength)
        {
            displayDesc = displayDesc.substr(0, maxLength - 3) + "...";
        }
        
        ImVec2 textSize = ImGui::CalcTextSize(displayDesc.c_str(), nullptr, true, cardWidth - 2 * cardPadding);
        cardContentHeight += textSize.y + 4; 
    }
    
    // Add height for metadata
    cardContentHeight += lineHeight + 4;
    
    // Use simple conditional instead of std::max to avoid Windows macro conflicts
    if (cardContentHeight < minCardHeight)
    {
        cardContentHeight = minCardHeight;
    }
    
    // Draw card background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 cursorPos = ImGui::GetCursorPos();
    
    ImVec2 cardMin = ImVec2(windowPos.x + cursorPos.x, windowPos.y + cursorPos.y);
    ImVec2 cardMax = ImVec2(cardMin.x + cardWidth, cardMin.y + cardContentHeight);
    
    // Card shadow effect
    ImVec2 shadowOffset = ImVec2(2, 2);
    drawList->AddRectFilled(
        ImVec2(cardMin.x + shadowOffset.x, cardMin.y + shadowOffset.y),
        ImVec2(cardMax.x + shadowOffset.x, cardMax.y + shadowOffset.y),
        IM_COL32(0, 0, 0, 40), 6.0f
    );
    
    // Card background and border
    drawList->AddRectFilled(cardMin, cardMax, cardBgColor, 6.0f);
    drawList->AddRect(cardMin, cardMax, cardBorderColor, 6.0f, 0, 1.5f);
    
    // Begin card content
    ImGui::PushID(card->id.c_str());
    
    // Position cursor for content
    ImGui::SetCursorPos(ImVec2(cursorPos.x + cardPadding, cursorPos.y + cardPadding));
    
    // Card title - always wrap text for responsive cards
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Could use bold font if available
    
    ImGui::PushTextWrapPos(cursorPos.x + cardWidth - cardPadding);
    ImGui::Text("%s", card->title.c_str());
    ImGui::PopTextWrapPos();
    
    ImGui::PopFont();
    ImGui::PopStyleColor();
    
    // Card description - responsive truncation and wrapping
    if (!card->description.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
        ImGui::PushTextWrapPos(cursorPos.x + cardWidth - cardPadding);
        
        // Adjust truncation based on card width
        std::string displayDesc = card->description;
        size_t maxLength = cardWidth < 100.0f ? 30 : (cardWidth < 150.0f ? 60 : 100);
        
        if (displayDesc.length() > maxLength)
        {
            displayDesc = displayDesc.substr(0, maxLength - 3) + "...";
        }
        
        ImGui::Text("%s", displayDesc.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }
    
    // Priority and metadata on the bottom - responsive format
    ImGui::SetCursorPos(ImVec2(cursorPos.x + cardPadding, cardMin.y + cardContentHeight - windowPos.y - lineHeight - cardPadding));
    
    // Priority and metadata - adjust format for narrow cards
    if (cardWidth < 80.0f)
    {
        // Use priority symbols for very narrow cards
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(priorityColor.r, priorityColor.g, priorityColor.b, 1.0f));
        ImGui::Text("●");
        ImGui::PopStyleColor();
        
        // Due date as just an icon for narrow cards
        if (!card->dueDate.empty())
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
            ImGui::Text("📅");
            ImGui::PopStyleColor();
        }
    }
    else
    {
        // Normal priority display for wider cards
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(priorityColor.r, priorityColor.g, priorityColor.b, 1.0f));
        if (cardWidth < 120.0f)
        {
            // Shorter priority names for medium-narrow cards
            const char* shortPriorityNames[] = {"Low", "Med", "High", "Urg"};
            int priorityIndex = static_cast<int>(card->priority);
            if (priorityIndex >= 0 && priorityIndex < 4)
            {
                ImGui::Text("● %s", shortPriorityNames[priorityIndex]);
            }
            else
            {
                ImGui::Text("● Med"); // Fallback
            }
        }
        else
        {
            // Full priority names for wider cards
            ImGui::Text("● %s", GetPriorityName(static_cast<int>(card->priority)));
        }
        ImGui::PopStyleColor();
        
        // Due date handling
        if (!card->dueDate.empty())
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
            if (cardWidth < 120.0f)
            {
                ImGui::Text("📅");
            }
            else
            {
                ImGui::Text("📅 %s", card->dueDate.c_str());
            }
            ImGui::PopStyleColor();
        }
    }
    
    // Position cursor after card
    ImGui::SetCursorPos(ImVec2(cursorPos.x, cursorPos.y + cardContentHeight + 4));
    
    // Invisible button for interaction (covering the entire card)
    ImVec2 cardRegionMin = ImVec2(cursorPos.x, cursorPos.y);
    ImVec2 cardRegionMax = ImVec2(cursorPos.x + cardWidth, cursorPos.y + cardContentHeight);
    
    ImGui::SetCursorPos(cardRegionMin);
    bool cardClicked = ImGui::InvisibleButton("##cardbutton", ImVec2(cardWidth, cardContentHeight));
    
    // Hover effect
    if (ImGui::IsItemHovered())
    {
        drawList->AddRect(cardMin, cardMax, IM_COL32(255, 255, 255, 100), 6.0f, 0, 2.0f);
    }
    
    // Double-click to edit
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        StartEditingCard(card);
    }
    
    // Drag and drop
    if (ImGui::BeginDragDropSource())
    {
        m_kanbanManager->StartDrag(card, columnId);
        ImGui::SetDragDropPayload("KANBAN_CARD", card.get(), sizeof(Kanban::Card*));
        
        // Custom drag preview
        ImGui::BeginTooltip();
        ImGui::Text("Moving: %s", card->title.c_str());
        if (!card->description.empty())
        {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%.50s%s", 
                             card->description.c_str(), 
                             card->description.length() > 50 ? "..." : "");
        }
        ImGui::EndTooltip();
        
        ImGui::EndDragDropSource();
    }
    
    // Context menu
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("✏️ Edit"))
        {
            StartEditingCard(card);
        }
        
        ImGui::Separator();
        
        if (ImGui::MenuItem("🗑️ Delete", nullptr, false, true))
        {
            m_kanbanManager->DeleteCard(card->id);
        }
        
        ImGui::EndPopup();
    }
    
    // Drop target between cards (only if not the first card)
    if (cardIndex > 0)
    {
        RenderDropTarget(columnId, cardIndex);
    }
    
    // Suppress unused parameter warning for cardIndex since it's used in the condition above
    (void)cardIndex;
    
    ImGui::PopID();
}

void MainWindow::RenderDropTarget(const std::string& columnId, int insertIndex)
{
    auto& dragState = m_kanbanManager->GetDragDropState();
    
    if (!dragState.isDragging)
        return;
    
    // Create a small invisible drop target
    float dropHeight = 20.0f;
    ImVec2 dropSize = ImVec2(ImGui::GetContentRegionAvail().x, dropHeight);
    
    ImGui::InvisibleButton(("##drop" + columnId + std::to_string(insertIndex)).c_str(), dropSize);
    
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("KANBAN_CARD"))
        {
            m_kanbanManager->UpdateDrag(columnId, insertIndex);
            m_kanbanManager->EndDrag();
        }
        else
        {
            m_kanbanManager->UpdateDrag(columnId, insertIndex);
        }
        
        // Visual feedback for drop target
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 itemMin = ImGui::GetItemRectMin();
        ImVec2 itemMax = ImGui::GetItemRectMax();
        
        drawList->AddRectFilled(itemMin, itemMax, IM_COL32(100, 150, 255, 100), 3.0f);
        drawList->AddRect(itemMin, itemMax, IM_COL32(100, 150, 255, 255), 3.0f, 0, 2.0f);
        
        ImGui::EndDragDropTarget();
    }
}

void MainWindow::RenderCardEditDialog()
{
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    
    bool isOpen = m_cardEditState.isEditing;
    if (ImGui::Begin("Edit Card", &isOpen, ImGuiWindowFlags_Modal))
    {
        ImGui::Text("Card Details");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Title
        ImGui::Text("Title:");
        ImGui::InputText("##title", m_cardEditState.titleBuffer, sizeof(m_cardEditState.titleBuffer));
        
        // Description
        ImGui::Text("Description:");
        ImGui::InputTextMultiline("##description", m_cardEditState.descriptionBuffer, 
                                 sizeof(m_cardEditState.descriptionBuffer), ImVec2(-1, 100));
        
        // Priority
        ImGui::Text("Priority:");
        const char* priorityNames[] = { "Low", "Medium", "High", "Urgent" };
        ImGui::Combo("##priority", &m_cardEditState.priority, priorityNames, IM_ARRAYSIZE(priorityNames));
        
        // Due date
        ImGui::Text("Due Date:");
        ImGui::InputText("##duedate", m_cardEditState.dueDateBuffer, sizeof(m_cardEditState.dueDateBuffer));
        ImGui::SameLine();
        if (ImGui::SmallButton("Today"))
        {
            // Set to today's date - simplified
            strcpy_s(m_cardEditState.dueDateBuffer, "Today");
        }
        
        // Assignee
        ImGui::Text("Assignee:");
        ImGui::InputText("##assignee", m_cardEditState.assigneeBuffer, sizeof(m_cardEditState.assigneeBuffer));
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Buttons
        if (ImGui::Button("Save", ImVec2(100, 0)))
        {
            StopEditingCard(true);
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
        {
            StopEditingCard(false);
        }
        
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - 100);
        if (ImGui::Button("Delete", ImVec2(100, 0)))
        {
            // Delete the card
            m_kanbanManager->DeleteCard(m_cardEditState.cardId);
            StopEditingCard(false);
        }
    }
    else
    {
        isOpen = false;
    }
    ImGui::End();
    
    if (!isOpen)
    {
        StopEditingCard(false);
    }
}

void MainWindow::StartEditingCard(std::shared_ptr<Kanban::Card> card)
{
    if (!card) return;
    
    m_cardEditState.isEditing = true;
    m_cardEditState.cardId = card->id;
    
    // Copy card data to edit buffers
    strncpy_s(m_cardEditState.titleBuffer, card->title.c_str(), sizeof(m_cardEditState.titleBuffer) - 1);
    strncpy_s(m_cardEditState.descriptionBuffer, card->description.c_str(), sizeof(m_cardEditState.descriptionBuffer) - 1);
    strncpy_s(m_cardEditState.dueDateBuffer, card->dueDate.c_str(), sizeof(m_cardEditState.dueDateBuffer) - 1);
    strncpy_s(m_cardEditState.assigneeBuffer, card->assignee.c_str(), sizeof(m_cardEditState.assigneeBuffer) - 1);
    
    m_cardEditState.priority = static_cast<int>(card->priority);
}

void MainWindow::StopEditingCard(bool save)
{
    if (save && !m_cardEditState.cardId.empty())
    {
        auto card = m_kanbanManager->FindCard(m_cardEditState.cardId);
        if (card)
        {
            // Update card with edited data
            card->title = m_cardEditState.titleBuffer;
            card->description = m_cardEditState.descriptionBuffer;
            card->dueDate = m_cardEditState.dueDateBuffer;
            card->assignee = m_cardEditState.assigneeBuffer;
            card->priority = static_cast<Kanban::Priority>(m_cardEditState.priority);
            
            m_kanbanManager->UpdateCard(card);
        }
    }
    
    // Clear edit state
    m_cardEditState = CardEditState();
}

void MainWindow::HandleCardDragDrop(std::shared_ptr<Kanban::Card> card, const std::string& columnId, int cardIndex)
{
    // This method can be used for additional drag/drop logic if needed
    // Currently handled in RenderKanbanCard and RenderDropTarget
}

// Kanban event handlers
void MainWindow::OnKanbanCardUpdated(std::shared_ptr<Kanban::Card> card)
{
    if (card)
    {
        Logger::Debug("Kanban card updated: {}", card->title);
    }
}

void MainWindow::OnKanbanBoardChanged(Kanban::Board* board)
{
    if (board)
    {
        Logger::Debug("Kanban board changed: {}", board->name);
    }
}

void MainWindow::OnKanbanProjectChanged(Kanban::Project* project)
{
    if (project)
    {
        Logger::Debug("Kanban project changed: {}", project->name);
    }
}

// Pomodoro Module Implementation
void MainWindow::RenderPomodoroModule()
{
    if (!m_pomodoroTimer)
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));

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

    ImGui::Spacing();
    if (ImGui::Button("Settings"))
    {
        m_showPomodoroSettings = true;
    }

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
    float buttonWidth = 24.0f;
    float buttonHeight = 24.0f;
    ImVec2 buttonSize = ImVec2(buttonWidth, buttonHeight);
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
        if (ImGui::ImageButton(iconPlay, buttonSize))
        {
            m_pomodoroTimer->Start();
            OnPomodoroSessionStart();
        }
    }
    else if (state == PomodoroTimer::TimerState::Running)
    {
        ImGui::PopStyleColor(2);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.7f, 0.2f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.8f, 0.3f, 1.0f));
        
        if (ImGui::ImageButton(iconPause, buttonSize))
            m_pomodoroTimer->Pause();
    }
    else if (state == PomodoroTimer::TimerState::Paused)
    {
        if (ImGui::ImageButton(iconPlay, buttonSize))
            m_pomodoroTimer->Resume();
    }
    
    ImGui::PopStyleColor(2);
    
    ImGui::SameLine(0, spacing);
    
    // Stop button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    
    if (ImGui::ImageButton(iconStop, buttonSize))
        m_pomodoroTimer->Stop();
    
    ImGui::PopStyleColor(2);
    
    ImGui::SameLine(0, spacing);
    
    // Skip button
    bool canSkip = (state == PomodoroTimer::TimerState::Running || 
                   state == PomodoroTimer::TimerState::Paused);
    
    if (!canSkip) ImGui::BeginDisabled();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.7f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.8f, 1.0f));
    
    if (ImGui::ImageButton(iconSkip, buttonSize))
        m_pomodoroTimer->Skip();
    
    ImGui::PopStyleColor(2);
    
    if (!canSkip) ImGui::EndDisabled();
    
    ImGui::SameLine(0, spacing);
    
    // Reset button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    
    if (ImGui::ImageButton(iconReset, buttonSize))
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
        
        // if (configChanged) // Legacy code
        // {
        //     m_pomodoroTimer->SetConfig(config);
            
        //     // Save to config file
        //     if (m_config)
        //     {
        //         m_config->SetValue("pomodoro.work_duration", config.workDurationMinutes);
        //         m_config->SetValue("pomodoro.short_break", config.shortBreakMinutes);
        //         m_config->SetValue("pomodoro.long_break", config.longBreakMinutes);
        //         m_config->SetValue("pomodoro.total_sessions", config.totalSessions);
        //         m_config->SetValue("pomodoro.auto_start_next", config.autoStartNextSession);
        //         m_config->Save();
        //     }
        // }

        if (configChanged)
        {
            m_pomodoroTimer->SetConfig(config);
            
            // Save to database instead of just AppConfig
            SavePomodoroConfiguration(config);
        }
    }
}

// Pomodoro Event Handlers
void MainWindow::OnPomodoroSessionComplete(int sessionType)
{
    std::string message;
    std::string sessionTypeStr;
    
    if (sessionType == 0) // Work session
    {
        message = "Work session completed! Time for a break.";
        sessionTypeStr = "work";
    }
    else if (sessionType == 1) // Short break
    {
        message = "Short break finished! Back to work.";
        sessionTypeStr = "short_break";
    }
    else if (sessionType == 2) // Long break
    {
        message = "Long break finished! Ready for more work.";
        sessionTypeStr = "long_break";
    }
    else
    {
        message = "Session completed!";
        sessionTypeStr = "unknown";
    }
    
    // End current session in database
    if (m_currentSessionId != -1 && m_pomodoroDatabase)
    {
        auto sessionInfo = m_pomodoroTimer->GetCurrentSession();
        int pausedSeconds = static_cast<int>(sessionInfo.pausedTime.count());
        
        m_pomodoroDatabase->EndSession(m_currentSessionId, true, pausedSeconds);
        m_currentSessionId = -1;
    }
    
    Logger::Info("Pomodoro: {}", message);
}

void MainWindow::OnPomodoroAllComplete()
{
    Logger::Info("Pomodoro: All sessions completed! Excellent work!");
    
    // End current session if any
    if (m_currentSessionId != -1 && m_pomodoroDatabase)
    {
        auto sessionInfo = m_pomodoroTimer->GetCurrentSession();
        int pausedSeconds = static_cast<int>(sessionInfo.pausedTime.count());
        
        m_pomodoroDatabase->EndSession(m_currentSessionId, true, pausedSeconds);
        m_currentSessionId = -1;
    }
}

void MainWindow::OnPomodoroTick()
{
    // Could be used for animations or updates
}

// Helper methods
ImVec4 MainWindow::GetPriorityColor(int priority) const
{
    switch (priority)
    {
        case 0: return ImVec4(0.5f, 0.8f, 0.5f, 1.0f); // Low - Green
        case 1: return ImVec4(0.8f, 0.8f, 0.5f, 1.0f); // Medium - Yellow
        case 2: return ImVec4(0.9f, 0.6f, 0.3f, 1.0f); // High - Orange
        case 3: return ImVec4(0.9f, 0.3f, 0.3f, 1.0f); // Urgent - Red
        default: return ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Default - Gray
    }
}

const char* MainWindow::GetPriorityName(int priority) const
{
    switch (priority)
    {
        case 0: return "Low";
        case 1: return "Medium";
        case 2: return "High";
        case 3: return "Urgent";
        default: return "Unknown";
    }
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

//////////////////////////
// File staging module API
//////////////////////////
void MainWindow::AddStagedFile(const std::string& path)
{
    m_stagedFiles.emplace_back(FileItem(path));
}

const std::vector<FileItem>& MainWindow::GetStagedFiles() const
{
    return m_stagedFiles;
}

// Private
std::string MainWindow::BrowseForFolder(HWND hwndOwner)  // no default here
{
    char path[MAX_PATH] = {0};

    BROWSEINFO bi = {0};
    bi.hwndOwner = hwndOwner;
    bi.lpszTitle = "Select Destination Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl)
    {
        if (SHGetPathFromIDListA(pidl, path))
        {
            CoTaskMemFree(pidl);
            return std::string(path);
        }
        CoTaskMemFree(pidl);
    }
    return "";
}

bool MainWindow::CopyFileWithProgress(
    const std::filesystem::path &src, const std::filesystem::path &dst,
    std::function<void(float)> progressCallback) 
{
  const size_t bufferSize = 1024 * 1024; // 1 MB buffer
  std::ifstream in(src, std::ios::binary);
  std::ofstream out(dst, std::ios::binary);

  if (!in || !out)
    return false;

  in.seekg(0, std::ios::end);
  size_t totalSize = in.tellg();
  in.seekg(0, std::ios::beg);

  size_t copied = 0;
  std::vector<char> buffer(bufferSize);

  while (in) {
    in.read(buffer.data(), bufferSize);
    std::streamsize readBytes = in.gcount();
    out.write(buffer.data(), readBytes);
    copied += readBytes;

    if (progressCallback)
      progressCallback(static_cast<float>(copied) / totalSize);
  }

  return true;
}

bool MainWindow::MoveFileOrDirWithProgress(
    const std::filesystem::path &src, const std::filesystem::path &dst,
    std::function<void(float)> progressCallback) 
{
  try {
    // Try atomic rename first
    std::filesystem::rename(src, dst);
    if (progressCallback)
      progressCallback(1.0f); // finished
    return true;
  } catch (...) {
    // Fallback to copy + delete
    if (std::filesystem::is_directory(src)) {
      size_t totalFiles =
          std::distance(std::filesystem::recursive_directory_iterator(src),
                        std::filesystem::recursive_directory_iterator{});
      size_t processedFiles = 0;

      for (auto &entry : std::filesystem::recursive_directory_iterator(src)) {
        std::filesystem::path relative = entry.path().lexically_relative(src);
        std::filesystem::path target = dst / relative;

        if (entry.is_directory())
          std::filesystem::create_directories(target);
        else
          CopyFileWithProgress(entry.path(), target,
                               nullptr); // we can refine for per-file progress

        processedFiles++;
        if (progressCallback)
          progressCallback(static_cast<float>(processedFiles) / totalFiles);
      }

      std::filesystem::remove_all(src);
      if (progressCallback)
        progressCallback(1.0f);
    } else {
      CopyFileWithProgress(src, dst, progressCallback);
      std::filesystem::remove(src);
    }
    return true;
  }
}

bool MainWindow::IsCriticalFileOrDir(const std::string &path) 
{ 
    return IsSystemPath(path) || IsProtectedFile(path);
}

bool MainWindow::IsSystemPath(const std::string &path) 
{ 
    char systemDir[MAX_PATH] = {0};
  if (GetSystemDirectoryA(systemDir, MAX_PATH) == 0)
    return false;

  std::string lowerPath = path;
  std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(),
                 ::tolower);
  std::string lowerSystemDir = systemDir;
  std::transform(lowerSystemDir.begin(), lowerSystemDir.end(),
                 lowerSystemDir.begin(), ::tolower);

  // Check if the path is inside the system directory
  return lowerPath.find(lowerSystemDir) == 0;
}

bool MainWindow::IsProtectedFile(const std::string &path) 
{ 
    DWORD attrs = GetFileAttributesA(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES)
    return false;

    return (attrs & FILE_ATTRIBUTE_SYSTEM) || (attrs & FILE_ATTRIBUTE_READONLY);
}

void MainWindow::MoveToRecycleBin(
    const std::vector<std::filesystem::path> &files)
{
  // Check staged files first
  for (auto &f : files) {
    if (IsCriticalFileOrDir(f.string())) {
      Logger::Error("Attempted to delete critical system file: " + f.string());
      // TODO: Show warning popup in ImGui
      return;
    }
  }

#ifdef _WIN32
  // Concatenate all paths separated by '\0' and double-terminate
  std::wstring fileList;
  for (auto &f : files) {
    fileList += f.wstring();
    fileList.push_back(L'\0');
  }
  fileList.push_back(L'\0');

  SHFILEOPSTRUCTW fileOp = {0};
  fileOp.wFunc = FO_DELETE;
  fileOp.pFrom = fileList.c_str();
  fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
  // FOF_ALLOWUNDO = send to Recycle Bin
  // NOCONFIRMATION = no "are you sure?"
  // SILENT = no UI dialogs

  int result = SHFileOperationW(&fileOp);
  if (result == 0) {
    Logger::Info("Files moved to Recycle Bin successfully.");
    // Optional: Show Windows toast notification
  } else {
    Logger::Error("Failed to move files to Recycle Bin. Code: " +
                  std::to_string(result));
  }
#endif
}

//////////////////////////
//
//////////////////////////

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
    
    /*
    * @note Paste (Copy)
    */
    ImGui::SameLine();
    if (ImGui::ImageButton(iconPaste, ImVec2(16, 16)) && !m_pasteInProgress) {
      Logger::Info("Paste Files clicked");

      std::string destFolder = BrowseForFolder();
      if (destFolder.empty()) {
        Logger::Info("Paste canceled, no folder selected");
      } else {
        m_pasteInProgress = true;
        m_pasteProgress = 0.0f;

        for (auto &file : m_stagedFiles) {
          if (IsCriticalFileOrDir(file.fullPath)) {
            Logger::Error("Cannot paste critical system file/folder: " +
                          file.fullPath);
            continue;
          }

          std::filesystem::path src(file.fullPath);
          std::filesystem::path dst =
              std::filesystem::path(destFolder) / src.filename();

          CopyFileWithProgress(src, dst, [&](float progress) {
            m_pasteProgress = progress; // update progress
          });

          Logger::Info("Copied: " + src.string() + " -> " + dst.string());

          // 🔄 Update staged file to new location
          file.fullPath = dst.string();
          file.name = Utils::GetFileName(dst.string());
          file.isDirectory = Utils::DirectoryExists(dst.string());
          if (!file.isDirectory) {
            uint64_t sizeBytes = Utils::GetFileSize(dst.string());
            file.size = Utils::FormatBytes(sizeBytes);
            file.type = Utils::GetFileExtension(dst.string());
          } else {
            file.size = "Folder";
            file.type = "Folder";
          }
          file.modified = "Recently"; // or refresh from metadata
        }

        m_pasteInProgress = false;
        m_pasteProgress = 0.0f;

        // Windows native notification
        Utils::ShowPasteCompleteNotification(destFolder);
      }
    }

    if (m_pasteInProgress) {
      ImGui::ProgressBar(m_pasteProgress, ImVec2(-1, 0), "Pasting files...");
    }

    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Paste Files");
    
    /*
    * @note Paste (Cut)
    */ 
    ImGui::SameLine();
    if (ImGui::ImageButton(iconCut, ImVec2(16, 16)) && !m_cutInProgress) {
      Logger::Info("Cut Files clicked");

      std::string destFolder = BrowseForFolder();
      if (destFolder.empty()) {
        Logger::Info("Cut canceled, no folder selected");
      } else {
        m_cutInProgress = true;
        m_cutProgress = 0.0f;

        for (auto &file : m_stagedFiles) {
          if (IsCriticalFileOrDir(file.fullPath)) {
            Logger::Error("Cannot cut critical system file/folder: " +
                          file.fullPath);
            continue;
          }

          std::filesystem::path src(file.fullPath);
          std::filesystem::path dst =
              std::filesystem::path(destFolder) / src.filename();

          MoveFileOrDirWithProgress(src, dst, [&](float progress) {
            m_cutProgress = progress; // update progress
          });

          Logger::Info("Moved: " + src.string() + " -> " + dst.string());

          // 🔄 Update staged file to new location
          file.fullPath = dst.string();
          file.name = Utils::GetFileName(dst.string());
          file.isDirectory = Utils::DirectoryExists(dst.string());
          if (!file.isDirectory) {
            uint64_t sizeBytes = Utils::GetFileSize(dst.string());
            file.size = Utils::FormatBytes(sizeBytes);
            file.type = Utils::GetFileExtension(dst.string());
          } else {
            file.size = "Folder";
            file.type = "Folder";
          }
          file.modified = "Recently";
        }

        m_cutInProgress = false;
        m_cutProgress = 0.0f;

        //ShowPasteCompleteNotification(destFolder);
        Utils::ShowPasteCompleteNotification(destFolder);
      }
    }

    if (m_cutInProgress) {
      ImGui::ProgressBar(m_cutProgress, ImVec2(-1, 0), "Cutting files...");
    }

    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Cut Files");

    /*
    * @note Rename
    */
    ImGui::SameLine();
    if (ImGui::ImageButton(iconRename, ImVec2(16, 16))) {
      Logger::Info("Bulk Rename clicked");
      showBulkRenamePopup = true;
      renameBuffer[0] = '\0'; // reset input buffer
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Bulk Rename");

    // Show modal popup
    if (showBulkRenamePopup)
      ImGui::OpenPopup("Bulk Rename");

    if (ImGui::BeginPopupModal("Bulk Rename", &showBulkRenamePopup,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Enter new base name:");
      ImGui::InputText("##renameInput", renameBuffer,
                       IM_ARRAYSIZE(renameBuffer));

      if (ImGui::Button("OK")) {
        renameError.clear();

        if (strlen(renameBuffer) == 0) {
          renameError = "Base name cannot be empty.";
        } else if (m_stagedFiles.empty()) {
          renameError = "No files staged.";
        } else {
          // Check if all staged files are in the same directory
          std::filesystem::path firstDir =
              std::filesystem::path(m_stagedFiles[0].fullPath).parent_path();
          bool sameDir = true;
          for (const auto &file : m_stagedFiles) {
            if (std::filesystem::path(file.fullPath).parent_path() !=
                firstDir) {
              sameDir = false;
              break;
            }
          }

          if (!sameDir) {
            renameError = "Files must be in the same directory.";
          } else {
            // Perform bulk rename
            int counter = 1;
            for (const auto &file : m_stagedFiles) {
              std::filesystem::path oldPath = file.fullPath;
              std::filesystem::path newPath =
                  firstDir /
                  (std::string(renameBuffer) + "-" + std::to_string(counter) +
                   oldPath.extension().string());

              try {
                std::filesystem::rename(oldPath, newPath);
                Logger::Info("Renamed " + oldPath.string() + " -> " +
                             newPath.string());
              } catch (const std::exception &ex) {
                Logger::Error("Failed to rename " + oldPath.string() + ": " +
                              ex.what());
              }
              counter++;
            }
            showBulkRenamePopup = false; // close popup
          }
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        showBulkRenamePopup = false;
      }

      if (!renameError.empty()) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", renameError.c_str());
      }

      ImGui::EndPopup();
    }
    
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    /*
    * @note Clear
    */
    if (ImGui::ImageButton(iconClear, ImVec2(16, 16)))
    {
        Logger::Info("Clear All clicked");
        m_stagedFiles.clear();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear All");
    
    /*
    * @note Move to recycle bin
    */
    ImGui::SameLine();
    if (ImGui::ImageButton(iconTrash, ImVec2(16, 16))) {
      // Assuming FileItem has a member `std::filesystem::path path;`
      std::vector<std::filesystem::path> paths;
      paths.reserve(m_stagedFiles.size());

      bool hasCriticalFileOrDir = false;

      for (const auto &item : m_stagedFiles) {
        paths.push_back(
            item.fullPath); // or item.GetPath() if you have a getter
        if (IsCriticalFileOrDir(item.fullPath))
          hasCriticalFileOrDir = true;
      }

      // Now you can call
      if (!hasCriticalFileOrDir) // cross-check first
      {
        MoveToRecycleBin(paths);
      } else {
        Logger::Warning("Attempted to delete critical files!");
        // TODO: Show warning dialog to user
      }
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Recycle Bin");

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
    // Add sample files for demonstration
    if (m_stagedFiles.empty())
    {
        // These would be actual files dropped by user
        //m_stagedFiles.push_back(FileItem("C:\\Windows\\System32\\notepad.exe"));
        //m_stagedFiles.push_back(FileItem("C:\\Windows\\System32"));
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
        for (size_t i = 0; i < m_stagedFiles.size(); ++i)
        {
            const FileItem& file = m_stagedFiles[i];
            
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
                    m_stagedFiles.erase(m_stagedFiles.begin() + i);
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
        if (m_stagedFiles.empty())
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
    ImGui::Text("Files staged: %zu", m_stagedFiles.size());
    if (!m_stagedFiles.empty())
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

    // Pomodoro icon
    iconPlay = LoadTextureFromResource(IDI_TIMER_PLAY);
    iconPause = LoadTextureFromResource(IDI_TIMER_PAUSE);
    iconReset = LoadTextureFromResource(IDI_TIMER_RESET);
    iconSkip = LoadTextureFromResource(IDI_TIMER_SKIP);
    iconStop = LoadTextureFromResource(IDI_TIMER_STOP);
}

// Placeholder methods for other modules
// void MainWindow::RenderClipboardPlaceholder()
// {
//     ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    
//     ImGui::Text("Clipboard Manager");
//     ImGui::Separator();
//     ImGui::Spacing();
    
//     ImGui::TextWrapped("The Clipboard Manager will be implemented in Sprint 3.");
//     ImGui::Spacing();
//     ImGui::Text("Features coming soon:");
//     ImGui::BulletText("Clipboard history");
//     ImGui::BulletText("Search and filter");
//     ImGui::BulletText("Favorites");
//     ImGui::BulletText("Auto-cleanup");
//     ImGui::BulletText("Quick paste");
    
//     ImGui::PopStyleVar();
// }

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

// void MainWindow::RenderFileConverterPlaceholder()
// {
//     ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    
//     ImGui::Text("File Converter");
//     ImGui::Separator();
//     ImGui::Spacing();
    
//     ImGui::TextWrapped("File conversion and compression will be implemented in Sprint 5.");
//     ImGui::Spacing();
//     ImGui::Text("Features coming soon:");
//     ImGui::BulletText("PDF compression");
//     ImGui::BulletText("Image format conversion");
//     ImGui::BulletText("Image compression");
//     ImGui::BulletText("Batch processing");
//     ImGui::BulletText("Quality settings");
    
//     ImGui::PopStyleVar();
// }

// void MainWindow::renderSettingPlaceholder()
// {
//     ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    
//     ImGui::Text("Settings");
//     ImGui::Separator();
//     ImGui::Spacing();
    
//     ImGui::TextWrapped("Settings in Sprint 6.");
//     ImGui::Spacing();
//     ImGui::Text("Features coming soon:");
//     ImGui::BulletText("Theme");
//     ImGui::BulletText("Account");
//     ImGui::BulletText("Update");
    
//     ImGui::PopStyleVar(); 
// }

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

// =============================================================================
// TODO MODULE IMPLEMENTATION
// =============================================================================

void MainWindow::RenderTodoModule()
{
    if (!m_todoManager)
    {
        ImGui::Text("Todo Manager not available");
        return;
    }
    
    // Date picker popup
    RenderDatePicker();
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    
    // Header with navigation and statistics
    RenderTodoHeader();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Main content area with sidebar and task view
    float sidebarWidth = 200.0f;
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float contentWidth = availableWidth - sidebarWidth - 10.0f;
    
    // Sidebar
    ImGui::BeginChild("##TodoSidebar", ImVec2(sidebarWidth, 0), true);
    RenderTodoSidebar();
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Main content area
    ImGui::BeginChild("##TodoContent", ImVec2(contentWidth, 0), false);
    
    // Render based on current view mode
    auto viewMode = m_todoManager->GetViewMode();
    switch (viewMode)
    {
        case TodoManager::ViewMode::Daily:
            RenderTodoDailyView();
            break;
        case TodoManager::ViewMode::Weekly:
            RenderTodoWeeklyView();
            break;
        case TodoManager::ViewMode::Monthly:
            RenderTodoCalendarView();
            break;
    }
    
    ImGui::EndChild();
    
    // Task edit dialog (modal)
    if (m_taskEditState.isEditing)
    {
        RenderTaskEditDialog();
    }
    
    ImGui::PopStyleVar();
}

void MainWindow::RenderTodoHeader()
{
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float buttonAreaWidth = 350.0f; // Increased to accommodate date picker
    float infoAreaWidth = availableWidth - buttonAreaWidth - 20.0f;
    
    // Start horizontal layout
    ImGui::BeginGroup();
    
    // Info area
    ImGui::BeginChild("##TodoHeaderInfo", ImVec2(infoAreaWidth, 30.0f), false, 
                     ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    // Current date and statistics
    std::string currentDate = m_todoManager->GetCurrentDate();
    ImGui::Text("📅 %s", currentDate.c_str());
    
    ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
    
    int totalTasks = m_todoManager->GetTotalTaskCount();
    int completedTasks = m_todoManager->GetCompletedTaskCount();
    int pendingTasks = m_todoManager->GetPendingTaskCount();
    int overdueTasks = m_todoManager->GetOverdueTaskCount();
    
    ImGui::Text("Total:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%d", totalTasks);
    
    ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
    ImGui::Text("Completed:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "%d", completedTasks);
    
    ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
    ImGui::Text("Pending:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.5f, 1.0f), "%d", pendingTasks);
    
    if (overdueTasks > 0)
    {
        ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
        ImGui::Text("Overdue:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%d", overdueTasks);
    }
    
    // Completion rate
    if (totalTasks > 0)
    {
        float completionRate = m_todoManager->GetCompletionRate() * 100.0f;
        ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
        ImGui::Text("Progress:");
        ImGui::SameLine();
        
        ImVec4 progressColor;
        if (completionRate >= 80.0f)
            progressColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // Green
        else if (completionRate >= 50.0f)
            progressColor = ImVec4(0.8f, 0.8f, 0.2f, 1.0f); // Yellow
        else
            progressColor = ImVec4(0.8f, 0.4f, 0.2f, 1.0f); // Orange
        
        ImGui::TextColored(progressColor, "%.1f%%", completionRate);
    }
    
    ImGui::EndChild();
    
    // Navigation and view buttons
    ImGui::SameLine();
    
    float buttonSpacing = 8.0f;
    float buttonWidth = 70.0f;
    
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    
    // Previous day button
    if (ImGui::Button("◀ Prev", ImVec2(buttonWidth, 28.0f)))
    {
        std::string prevDate = m_todoManager->GetPreviousDay(m_todoManager->GetCurrentDate());
        m_todoManager->SetCurrentDate(prevDate);
    }
    
    ImGui::SameLine(0, buttonSpacing);
    
    // Today button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));
    
    if (ImGui::Button("Today", ImVec2(buttonWidth, 28.0f)))
    {
        m_todoManager->SetCurrentDate(m_todoManager->GetTodayDate());
    }
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine(0, buttonSpacing);
    
    // Next day button
    if (ImGui::Button("Next ▶", ImVec2(buttonWidth, 28.0f)))
    {
        std::string nextDate = m_todoManager->GetNextDay(m_todoManager->GetCurrentDate());
        m_todoManager->SetCurrentDate(nextDate);
    }
    
    ImGui::SameLine(0, buttonSpacing);
    
    // Date picker button (replaces the "Day" button)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.8f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.4f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.2f, 0.7f, 1.0f));
    
    if (ImGui::Button("📅 Pick Date", ImVec2(90.0f, 28.0f)))
    {
        // Initialize date picker with current date
        GetCurrentDateComponents(m_datePickerState.selectedYear, 
                               m_datePickerState.selectedMonth, 
                               m_datePickerState.selectedDay);
        m_datePickerState.displayYear = m_datePickerState.selectedYear;
        m_datePickerState.displayMonth = m_datePickerState.selectedMonth;
        m_datePickerState.isOpen = true;
    }
    ImGui::PopStyleColor(3);
    
    ImGui::PopStyleVar(); // FrameRounding
    
    ImGui::EndGroup();
}

void MainWindow::RenderTodoSidebar()
{
    ImGui::Text("📋 Quick Views");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Today tasks
    auto todayTasks = m_todoManager->GetTodayTasks();
    int todayPending = static_cast<int>(std::count_if(todayTasks.begin(), todayTasks.end(),
        [](const std::shared_ptr<Todo::Task>& task) { return task && !task->IsCompleted(); }));
    
    if (ImGui::Selectable(("📅 Today (" + std::to_string(todayPending) + ")").c_str()))
    {
        m_todoManager->SetCurrentDate(m_todoManager->GetTodayDate());
        m_todoManager->SetViewMode(TodoManager::ViewMode::Daily);
    }
    
    // Overdue tasks
    auto overdueTasks = m_todoManager->GetOverdueTasks();
    if (!overdueTasks.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Selectable(("⚠️ Overdue (" + std::to_string(overdueTasks.size()) + ")").c_str()))
        {
            // Show overdue tasks (could implement a special view)
        }
        ImGui::PopStyleColor();
    }
    
    // Upcoming tasks
    auto upcomingTasks = m_todoManager->GetUpcomingTasks(7);
    int upcomingPending = static_cast<int>(std::count_if(upcomingTasks.begin(), upcomingTasks.end(),
        [](const std::shared_ptr<Todo::Task>& task) { return task && !task->IsCompleted(); }));
    
    if (ImGui::Selectable(("📆 Upcoming (" + std::to_string(upcomingPending) + ")").c_str()))
    {
        m_todoManager->SetViewMode(TodoManager::ViewMode::Weekly);
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Statistics
    ImGui::Text("📊 Statistics");
    ImGui::Spacing();
    
    float completionRate = m_todoManager->GetCompletionRate() * 100.0f;
    ImGui::Text("Completion Rate:");
    ImGui::ProgressBar(m_todoManager->GetCompletionRate(), ImVec2(-1, 0), 
                      (std::to_string(static_cast<int>(completionRate)) + "%").c_str());
    
    ImGui::Spacing();
    ImGui::Text("Total Tasks: %d", m_todoManager->GetTotalTaskCount());
    ImGui::Text("Completed: %d", m_todoManager->GetCompletedTaskCount());
    ImGui::Text("Pending: %d", m_todoManager->GetPendingTaskCount());
    
    int overdueCount = m_todoManager->GetOverdueTaskCount();
    if (overdueCount > 0)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        ImGui::Text("Overdue: %d", overdueCount);
        ImGui::PopStyleColor();
    }
}

void MainWindow::RenderTodoDailyView()
{
    std::string currentDate = m_todoManager->GetCurrentDate();
    auto dayTasks = m_todoManager->GetTasksForDate(currentDate);
    
    // Date header with quick add
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Could use larger font
    ImGui::Text("📅 %s", currentDate.c_str());
    ImGui::PopFont();
    
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - 100);
    if (ImGui::Button("+ Add Task", ImVec2(100, 0)))
    {
        auto task = m_todoManager->CreateTask("New Task", currentDate);
        if (task)
        {
            StartEditingTask(task);
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Quick add task
    // RenderQuickAddTask(currentDate);
    
    // ImGui::Spacing();
    
    // Tasks list
    if (dayTasks && !dayTasks->tasks.empty())
    {
        RenderTodoTaskList(currentDate, dayTasks->tasks);
    }
    else
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 50);
        ImVec2 centerPos = ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 80, ImGui::GetCursorPosY());
        ImGui::SetCursorPos(centerPos);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No tasks for this day");
        ImGui::SetCursorPos(ImVec2(centerPos.x - 20, centerPos.y + 20));
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Add a task to get started!");
    }
}

void MainWindow::RenderTodoWeeklyView()
{
    // Implementation for weekly view
    ImGui::Text("Weekly View - Coming Soon");
    ImGui::Text("This will show a week layout with tasks for each day");
}

void MainWindow::RenderTodoCalendarView()
{
    // Implementation for calendar view
    ImGui::Text("Calendar View - Coming Soon");
    ImGui::Text("This will show a monthly calendar with task indicators");
}

void MainWindow::RenderTodoTaskList(const std::string& date, const std::vector<std::shared_ptr<Todo::Task>>& tasks)
{
    ImGui::BeginChild("##TaskList", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    
    for (size_t i = 0; i < tasks.size(); ++i)
    {
        auto task = tasks[i];
        if (!task) continue;
        
        if (i > 0)
        {
            ImGui::Spacing();
        }
        
        RenderTodoTask(task, static_cast<int>(i), date);
        
        // Drop target between tasks
        if (i < tasks.size() - 1)
        {
            RenderTodoDropTarget(date, static_cast<int>(i) + 1);
        }
    }
    
    // Drop target at the end
    RenderTodoDropTarget(date, static_cast<int>(tasks.size()));
    
    ImGui::EndChild();
}

void MainWindow::RenderTodoTask(std::shared_ptr<Todo::Task> task, int taskIndex, const std::string& date)
{
    if (!task) return;
    
    float taskWidth = ImGui::GetContentRegionAvail().x;
    float taskPadding = 8.0f;
    float minTaskHeight = 60.0f;
    
    // Task background color based on status and priority
    auto statusColor = task->GetStatusColor();
    auto priorityColor = task->GetPriorityColor();
    
    ImU32 taskBgColor;
    if (task->IsCompleted())
    {
        taskBgColor = IM_COL32(40, 60, 40, 255); // Darker green for completed
    }
    else if (task->IsOverdue())
    {
        taskBgColor = IM_COL32(80, 40, 40, 255); // Dark red for overdue
    }
    else
    {
        taskBgColor = IM_COL32(
            static_cast<int>(priorityColor.x * 80 + 30),
            static_cast<int>(priorityColor.y * 80 + 30),
            static_cast<int>(priorityColor.z * 80 + 30),
            255
        );
    }
    
    ImU32 taskBorderColor = IM_COL32(
        static_cast<int>(priorityColor.x * 150 + 60),
        static_cast<int>(priorityColor.y * 150 + 60),
        static_cast<int>(priorityColor.z * 150 + 60),
        255
    );
    
    // Calculate task content height
    float lineHeight = ImGui::GetTextLineHeight();
    float taskContentHeight = taskPadding * 2 + lineHeight; // Title
    
    if (!task->description.empty())
    {
        ImVec2 textSize = ImGui::CalcTextSize(task->description.c_str(), nullptr, true, taskWidth - 2 * taskPadding);
        taskContentHeight += textSize.y + 4;
    }
    
    taskContentHeight += lineHeight + 4; // Metadata line
    
    if (taskContentHeight < minTaskHeight)
    {
        taskContentHeight = minTaskHeight;
    }
    
    // Draw task background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 cursorPos = ImGui::GetCursorPos();
    
    ImVec2 taskMin = ImVec2(windowPos.x + cursorPos.x, windowPos.y + cursorPos.y);
    ImVec2 taskMax = ImVec2(taskMin.x + taskWidth, taskMin.y + taskContentHeight);
    
    // Task shadow
    ImVec2 shadowOffset = ImVec2(2, 2);
    drawList->AddRectFilled(
        ImVec2(taskMin.x + shadowOffset.x, taskMin.y + shadowOffset.y),
        ImVec2(taskMax.x + shadowOffset.x, taskMax.y + shadowOffset.y),
        IM_COL32(0, 0, 0, 40), 6.0f
    );
    
    // Task background and border
    drawList->AddRectFilled(taskMin, taskMax, taskBgColor, 6.0f);
    drawList->AddRect(taskMin, taskMax, taskBorderColor, 6.0f, 0, 1.5f);
    
    // Begin task content
    ImGui::PushID(task->id.c_str());
    
    // Checkbox area
    ImGui::SetCursorPos(ImVec2(cursorPos.x + taskPadding, cursorPos.y + taskPadding));
    
    bool completed = task->IsCompleted();
    if (ImGui::Checkbox("##completed", &completed))
    {
        m_todoManager->ToggleTaskStatus(task->id);
    }
    
    // Task title
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);
    
    ImGui::PushStyleColor(ImGuiCol_Text, completed ? 
                         ImVec4(0.6f, 0.6f, 0.6f, 1.0f) : ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
    
    if (completed)
    {
        // Strikethrough effect for completed tasks
        ImGui::Text("✓ %s", task->title.c_str());
    }
    else
    {
        ImGui::Text("%s", task->title.c_str());
    }
    ImGui::PopStyleColor();
    
    // Task description
    if (!task->description.empty())
    {
        ImGui::SetCursorPosX(cursorPos.x + taskPadding + 25); // Indent for description
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
        ImGui::PushTextWrapPos(cursorPos.x + taskWidth - taskPadding);
        ImGui::Text("%s", task->description.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }
    
    // Task metadata (priority, due time, status)
    ImGui::SetCursorPos(ImVec2(cursorPos.x + taskPadding + 25, 
                              taskMin.y + taskContentHeight - windowPos.y - lineHeight - taskPadding));
    
    // Priority indicator
    ImGui::PushStyleColor(ImGuiCol_Text, priorityColor);
    ImGui::Text("● %s", GetPriorityName(static_cast<int>(task->priority)));
    ImGui::PopStyleColor();
    
    // Due time
    if (!task->dueTime.empty() && !task->isAllDay)
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
        ImGui::Text(" 🕐 %s", task->dueTime.c_str());
        ImGui::PopStyleColor();
    }
    
    // Status indicator
    if (task->status != Todo::Status::Pending)
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
        ImGui::Text(" [%s]", GetStatusName(static_cast<int>(task->status)));
        ImGui::PopStyleColor();
    }
    
    // Due date info
    if (task->IsOverdue())
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        ImGui::Text(" ⚠️ OVERDUE");
        ImGui::PopStyleColor();
    }
    
    // Position cursor after task
    ImGui::SetCursorPos(ImVec2(cursorPos.x, cursorPos.y + taskContentHeight + 4));
    
    // Invisible button for interaction
    ImGui::SetCursorPos(ImVec2(cursorPos.x, cursorPos.y));
    bool taskClicked = ImGui::InvisibleButton("##taskbutton", ImVec2(taskWidth, taskContentHeight));
    
    // Hover effect
    if (ImGui::IsItemHovered())
    {
        drawList->AddRect(taskMin, taskMax, IM_COL32(255, 255, 255, 100), 6.0f, 0, 2.0f);
    }
    
    // Double-click to toggle completion
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        m_todoManager->ToggleTaskStatus(task->id);
    }
    
    // Right-click context menu
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("✏️ Edit"))
        {
            StartEditingTask(task);
        }
        
        ImGui::Separator();
        
        if (task->IsCompleted())
        {
            if (ImGui::MenuItem("🔄 Mark as Pending"))
            {
                m_todoManager->ToggleTaskStatus(task->id);
            }
        }
        else
        {
            if (ImGui::MenuItem("✅ Mark as Completed"))
            {
                m_todoManager->ToggleTaskStatus(task->id);
            }
            
            if (ImGui::MenuItem("🔄 Mark as In Progress"))
            {
                task->status = Todo::Status::InProgress;
                m_todoManager->UpdateTask(task);
            }
        }
        
        ImGui::Separator();
        
        if (ImGui::MenuItem("🗑️ Delete", nullptr, false, true))
        {
            m_todoManager->DeleteTask(task->id);
        }
        
        ImGui::EndPopup();
    }
    
    // Drag and drop
    if (ImGui::BeginDragDropSource())
    {
        m_todoManager->StartDrag(task, date);
        ImGui::SetDragDropPayload("TODO_TASK", task.get(), sizeof(Todo::Task*));
        
        // Custom drag preview
        ImGui::BeginTooltip();
        ImGui::Text("Moving: %s", task->title.c_str());
        if (!task->description.empty())
        {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%.50s%s", 
                             task->description.c_str(), 
                             task->description.length() > 50 ? "..." : "");
        }
        ImGui::EndTooltip();
        
        ImGui::EndDragDropSource();
    }
    
    ImGui::PopID();
}

void MainWindow::RenderQuickAddTask(const std::string& date)
{
    static std::string quickAddText;
    static bool quickAddActive = false;
    
    if (!quickAddActive)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.5f, 0.8f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.4f, 0.7f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        
        if (ImGui::Button("+ Add Task", ImVec2(-1, 32.0f)))
        {
            quickAddActive = true;
            quickAddText = "";
        }
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }
    else
    {
        // Quick add input
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
        
        char buffer[256];
        strncpy_s(buffer, quickAddText.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        
        ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1);
        bool enterPressed = ImGui::InputText("##quickaddtask", buffer, sizeof(buffer), 
                                           ImGuiInputTextFlags_EnterReturnsTrue | 
                                           ImGuiInputTextFlags_AutoSelectAll);
        quickAddText = buffer;
        
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        
        if (enterPressed && strlen(buffer) > 0)
        {
            // Create the task
            m_todoManager->CreateTask(buffer, date);
            quickAddActive = false;
            quickAddText = "";
        }
        
        // Cancel on escape or click outside
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || 
            (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered()))
        {
            quickAddActive = false;
            quickAddText = "";
        }
        
        // Instruction text
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::Text("Enter to add, Esc to cancel");
        ImGui::PopStyleColor();
    }
}

void MainWindow::RenderTodoDropTarget(const std::string& date, int insertIndex)
{
    auto& dragState = m_todoManager->GetDragDropState();
    
    if (!dragState.isDragging)
        return;
    
    // Create a small invisible drop target
    float dropHeight = 20.0f;
    ImVec2 dropSize = ImVec2(ImGui::GetContentRegionAvail().x, dropHeight);
    
    ImGui::InvisibleButton(("##todoDrop" + date + std::to_string(insertIndex)).c_str(), dropSize);
    
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TODO_TASK"))
        {
            m_todoManager->UpdateDrag(date, insertIndex);
            m_todoManager->EndDrag();
        }
        else
        {
            m_todoManager->UpdateDrag(date, insertIndex);
        }
        
        // Visual feedback for drop target
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 itemMin = ImGui::GetItemRectMin();
        ImVec2 itemMax = ImGui::GetItemRectMax();
        
        drawList->AddRectFilled(itemMin, itemMax, IM_COL32(100, 150, 255, 100), 3.0f);
        drawList->AddRect(itemMin, itemMax, IM_COL32(100, 150, 255, 255), 3.0f, 0, 2.0f);
        
        ImGui::EndDragDropTarget();
    }
}

void MainWindow::RenderTaskEditDialog()
{
    ImGui::SetNextWindowSize(ImVec2(500, 450), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    
    bool isOpen = m_taskEditState.isEditing;
    if (ImGui::Begin("Edit Task", &isOpen, ImGuiWindowFlags_Modal))
    {
        ImGui::Text("Task Details");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Title
        ImGui::Text("Title:");
        ImGui::InputText("##title", m_taskEditState.titleBuffer, sizeof(m_taskEditState.titleBuffer));
        
        // Description
        ImGui::Text("Description:");
        ImGui::InputTextMultiline("##description", m_taskEditState.descriptionBuffer, 
                                 sizeof(m_taskEditState.descriptionBuffer), ImVec2(-1, 80));
        
        // Priority
        ImGui::Text("Priority:");
        const char* priorityNames[] = { "Low", "Medium", "High", "Urgent" };
        ImGui::Combo("##priority", &m_taskEditState.priority, priorityNames, IM_ARRAYSIZE(priorityNames));
        
        // Status
        ImGui::Text("Status:");
        const char* statusNames[] = { "Pending", "In Progress", "Completed", "Cancelled" };
        ImGui::Combo("##status", &m_taskEditState.status, statusNames, IM_ARRAYSIZE(statusNames));
        
        // Due date
        ImGui::Text("Due Date:");
        ImGui::InputText("##duedate", m_taskEditState.dueDateBuffer, sizeof(m_taskEditState.dueDateBuffer));
        ImGui::SameLine();
        if (ImGui::SmallButton("Today"))
        {
            strcpy_s(m_taskEditState.dueDateBuffer, m_todoManager->GetTodayDate().c_str());
        }
        
        // All day toggle
        ImGui::Checkbox("All day", &m_taskEditState.isAllDay);
        
        // Due time (if not all day)
        if (!m_taskEditState.isAllDay)
        {
            ImGui::Text("Due Time:");
            ImGui::InputText("##duetime", m_taskEditState.dueTimeBuffer, sizeof(m_taskEditState.dueTimeBuffer));
            ImGui::SameLine();
            if (ImGui::SmallButton("Now"))
            {
                // Set to current time - simplified
                strcpy_s(m_taskEditState.dueTimeBuffer, "12:00");
            }
        }
        
        // Category
        ImGui::Text("Category:");
        ImGui::InputText("##category", m_taskEditState.categoryBuffer, sizeof(m_taskEditState.categoryBuffer));
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Buttons
        if (ImGui::Button("Save", ImVec2(100, 0)))
        {
            StopEditingTask(true);
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
        {
            StopEditingTask(false);
        }
        
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - 100);
        if (ImGui::Button("Delete", ImVec2(100, 0)))
        {
            // Delete the task
            m_todoManager->DeleteTask(m_taskEditState.taskId);
            StopEditingTask(false);
        }
    }
    else
    {
        isOpen = false;
    }
    ImGui::End();
    
    if (!isOpen)
    {
        StopEditingTask(false);
    }
}

void MainWindow::StartEditingTask(std::shared_ptr<Todo::Task> task)
{
    if (!task) return;
    
    m_taskEditState.isEditing = true;
    m_taskEditState.taskId = task->id;
    
    // Copy task data to edit buffers
    strncpy_s(m_taskEditState.titleBuffer, task->title.c_str(), sizeof(m_taskEditState.titleBuffer) - 1);
    strncpy_s(m_taskEditState.descriptionBuffer, task->description.c_str(), sizeof(m_taskEditState.descriptionBuffer) - 1);
    strncpy_s(m_taskEditState.dueDateBuffer, task->dueDate.c_str(), sizeof(m_taskEditState.dueDateBuffer) - 1);
    strncpy_s(m_taskEditState.dueTimeBuffer, task->dueTime.c_str(), sizeof(m_taskEditState.dueTimeBuffer) - 1);
    strncpy_s(m_taskEditState.categoryBuffer, task->category.c_str(), sizeof(m_taskEditState.categoryBuffer) - 1);
    
    m_taskEditState.priority = static_cast<int>(task->priority);
    m_taskEditState.status = static_cast<int>(task->status);
    m_taskEditState.isAllDay = task->isAllDay;
}

void MainWindow::StopEditingTask(bool save)
{
    if (save && !m_taskEditState.taskId.empty())
    {
        auto task = m_todoManager->FindTask(m_taskEditState.taskId);
        if (task)
        {
            // Update task with edited data
            std::string oldDate = task->dueDate;
            
            task->title = m_taskEditState.titleBuffer;
            task->description = m_taskEditState.descriptionBuffer;
            task->dueDate = m_taskEditState.dueDateBuffer;
            task->dueTime = m_taskEditState.dueTimeBuffer;
            task->category = m_taskEditState.categoryBuffer;
            task->priority = static_cast<Todo::Priority>(m_taskEditState.priority);
            task->status = static_cast<Todo::Status>(m_taskEditState.status);
            task->isAllDay = m_taskEditState.isAllDay;
            
            // If date changed, move the task
            if (oldDate != task->dueDate)
            {
                m_todoManager->MoveTask(task->id, task->dueDate);
            }
            else
            {
                m_todoManager->UpdateTask(task);
            }
        }
    }
    
    // Clear edit state
    m_taskEditState = TaskEditState();
}

// Todo event handlers
void MainWindow::OnTodoTaskUpdated(std::shared_ptr<Todo::Task> task)
{
    if (task)
    {
        Logger::Debug("Todo task updated: {}", task->title);
    }
}

void MainWindow::OnTodoTaskCompleted(std::shared_ptr<Todo::Task> task)
{
    if (task)
    {
        Logger::Debug("Todo task completed: {}", task->title);
    }
}

void MainWindow::OnTodoDayChanged(const std::string& date)
{
    Logger::Debug("Todo day changed: {}", date);
}

// Helper methods for Todo
const char* MainWindow::GetStatusName(int status) const
{
    switch (status)
    {
        case 0: return "Pending";
        case 1: return "In Progress";
        case 2: return "Completed";
        case 3: return "Cancelled";
        default: return "Unknown";
    }
}

ImVec4 MainWindow::GetStatusColor(int status) const
{
    switch (status)
    {
        case 0: return ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray - Pending
        case 1: return ImVec4(0.3f, 0.6f, 0.9f, 1.0f); // Blue - In Progress
        case 2: return ImVec4(0.4f, 0.8f, 0.4f, 1.0f); // Green - Completed
        case 3: return ImVec4(0.8f, 0.4f, 0.4f, 1.0f); // Red - Cancelled
        default: return ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray
    }
}

// =============================================================================
// DATE PICKER IMPLEMENTATION
// =============================================================================

void MainWindow::GetCurrentDateComponents(int& year, int& month, int& day) const
{
    std::string currentDate = m_todoManager->GetCurrentDate();
    // Parse YYYY-MM-DD format
    if (currentDate.length() >= 10)
    {
        year = std::stoi(currentDate.substr(0, 4));
        month = std::stoi(currentDate.substr(5, 2)) - 1; // Convert to 0-based
        day = std::stoi(currentDate.substr(8, 2));
    }
}

std::string MainWindow::FormatDateComponents(int year, int month, int day) const
{
    char buffer[32];
    sprintf_s(buffer, "%04d-%02d-%02d", year, month + 1, day); // Convert from 0-based month
    return std::string(buffer);
}

int MainWindow::GetDaysInMonth(int year, int month) const
{
    const int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    if (month == 1) // February
    {
        // Check for leap year
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
            return 29;
        else
            return 28;
    }
    
    return daysInMonth[month];
}

int MainWindow::GetFirstDayOfWeek(int year, int month) const
{
    // Simple algorithm to get day of week for first day of month
    // This is a simplified version - you might want to use a more robust date library
    int totalDays = 0;
    
    // Days from year 1900 (known Sunday = 0)
    for (int y = 1900; y < year; y++)
    {
        totalDays += 365;
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))
            totalDays += 1; // Leap year
    }
    
    // Days from months in current year
    for (int m = 0; m < month; m++)
    {
        totalDays += GetDaysInMonth(year, m);
    }
    
    return totalDays % 7;
}

void MainWindow::RenderDatePicker()
{
    if (!m_datePickerState.isOpen)
        return;
    
    ImGui::SetNextWindowSize(ImVec2(320, 280), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    bool isOpen = m_datePickerState.isOpen;
    if (ImGui::Begin("📅 Select Date", &isOpen, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        // Month/Year navigation
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        
        // Previous month button
        if (ImGui::Button("◀"))
        {
            m_datePickerState.displayMonth--;
            if (m_datePickerState.displayMonth < 0)
            {
                m_datePickerState.displayMonth = 11;
                m_datePickerState.displayYear--;
            }
        }
        
        ImGui::SameLine();
        
        // Month/Year display and selection
        const char* monthNames[] = {
            "January", "February", "March", "April", "May", "June",
            "July", "August", "September", "October", "November", "December"
        };
        
        ImGui::SetNextItemWidth(120);
        if (ImGui::BeginCombo("##month", monthNames[m_datePickerState.displayMonth]))
        {
            for (int i = 0; i < 12; i++)
            {
                bool isSelected = (i == m_datePickerState.displayMonth);
                if (ImGui::Selectable(monthNames[i], isSelected))
                {
                    m_datePickerState.displayMonth = i;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        
        ImGui::SameLine();
        
        // Year input
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("##year", &m_datePickerState.displayYear, 1, 10);
        
        // Clamp year to reasonable range
        if (m_datePickerState.displayYear < 1900) m_datePickerState.displayYear = 1900;
        if (m_datePickerState.displayYear > 2100) m_datePickerState.displayYear = 2100;
        
        ImGui::SameLine();
        
        // Next month button
        if (ImGui::Button("▶"))
        {
            m_datePickerState.displayMonth++;
            if (m_datePickerState.displayMonth > 11)
            {
                m_datePickerState.displayMonth = 0;
                m_datePickerState.displayYear++;
            }
        }
        
        ImGui::PopStyleVar();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Calendar grid
        const char* dayNames[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
        
        // Day headers
        ImGui::Columns(7, "DayHeaders", false);
        for (int i = 0; i < 7; i++)
        {
            ImGui::Text("%s", dayNames[i]);
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
        
        ImGui::Separator();
        
        // Calendar days
        int daysInMonth = GetDaysInMonth(m_datePickerState.displayYear, m_datePickerState.displayMonth);
        int firstDayOfWeek = GetFirstDayOfWeek(m_datePickerState.displayYear, m_datePickerState.displayMonth);
        
        ImGui::Columns(7, "Calendar", false);
        
        // Empty cells before first day
        for (int i = 0; i < firstDayOfWeek; i++)
        {
            ImGui::NextColumn();
        }
        
        // Days of the month
        for (int day = 1; day <= daysInMonth; day++)
        {
            bool isSelected = (day == m_datePickerState.selectedDay &&
                             m_datePickerState.displayMonth == m_datePickerState.selectedMonth &&
                             m_datePickerState.displayYear == m_datePickerState.selectedYear);
            
            // Highlight today
            std::string todayDate = m_todoManager->GetTodayDate();
            std::string thisDate = FormatDateComponents(m_datePickerState.displayYear, m_datePickerState.displayMonth, day);
            bool isToday = (thisDate == todayDate);
            
            if (isToday)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
            }
            else if (isSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.6f, 0.9f, 1.0f));
            }
            
            char dayStr[8];
            sprintf_s(dayStr, "%d", day);
            
            if (ImGui::Button(dayStr, ImVec2(35, 25)))
            {
                m_datePickerState.selectedDay = day;
                m_datePickerState.selectedMonth = m_datePickerState.displayMonth;
                m_datePickerState.selectedYear = m_datePickerState.displayYear;
                
                // Apply the selected date
                std::string newDate = FormatDateComponents(m_datePickerState.selectedYear, 
                                                         m_datePickerState.selectedMonth, 
                                                         m_datePickerState.selectedDay);
                m_todoManager->SetCurrentDate(newDate);
                
                m_datePickerState.isOpen = false;
            }
            
            if (isToday || isSelected)
            {
                ImGui::PopStyleColor(2);
            }
            
            ImGui::NextColumn();
        }
        
        ImGui::Columns(1);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Quick navigation buttons
        float buttonWidth = 90.0f;
        float totalButtonWidth = 3 * buttonWidth + 2 * 10.0f; // 3 buttons + 2 spacing
        ImGui::SetCursorPosX((320 - totalButtonWidth) * 0.5f);
        
        if (ImGui::Button("Today", ImVec2(buttonWidth, 0)))
        {
            std::string today = m_todoManager->GetTodayDate();
            m_todoManager->SetCurrentDate(today);
            
            // Update picker state to today
            GetCurrentDateComponents(m_datePickerState.selectedYear, 
                                   m_datePickerState.selectedMonth, 
                                   m_datePickerState.selectedDay);
            m_datePickerState.displayYear = m_datePickerState.selectedYear;
            m_datePickerState.displayMonth = m_datePickerState.selectedMonth;
            
            m_datePickerState.isOpen = false;
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
        {
            m_datePickerState.isOpen = false;
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Apply", ImVec2(buttonWidth, 0)))
        {
            std::string newDate = FormatDateComponents(m_datePickerState.selectedYear, 
                                                     m_datePickerState.selectedMonth, 
                                                     m_datePickerState.selectedDay);
            m_todoManager->SetCurrentDate(newDate);
            m_datePickerState.isOpen = false;
        }
    }
    else
    {
        isOpen = false;
    }
    
    ImGui::End();
    
    if (!isOpen)
    {
        m_datePickerState.isOpen = false;
    }
}

// =============================================================================
// CLIPBOARD MODULE IMPLEMENTATION
// =============================================================================

void MainWindow::RenderClipboardModule()
{
    if (!m_clipboardManager)
    {
        ImGui::Text("Clipboard Manager not available");
        return;
    }
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    
    // Header with search and controls
    RenderClipboardHeader();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Main content area with list and preview
    float listWidth = ImGui::GetContentRegionAvail().x * 0.6f;
    float previewWidth = ImGui::GetContentRegionAvail().x * 0.4f - 10.0f;
    
    // Clipboard items list
    ImGui::BeginChild("##ClipboardList", ImVec2(listWidth, 0), true);
    RenderClipboardList();
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Preview panel
    if (m_clipboardUIState.showPreview)
    {
        ImGui::BeginChild("##ClipboardPreview", ImVec2(previewWidth, 0), true);
        RenderClipboardPreview();
        ImGui::EndChild();
    }
    
    ImGui::PopStyleVar();
}

void MainWindow::RenderClipboardHeader()
{
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float buttonAreaWidth = 280.0f;
    float searchAreaWidth = availableWidth - buttonAreaWidth - 20.0f;
    
    // Search area
    ImGui::BeginChild("##ClipboardHeaderSearch", ImVec2(searchAreaWidth, 30.0f), false, 
                     ImGuiWindowFlags_NoScrollbar);
    
    // Search box
    char searchBuffer[256];
    strncpy_s(searchBuffer, m_clipboardUIState.searchQuery.c_str(), sizeof(searchBuffer) - 1);
    searchBuffer[sizeof(searchBuffer) - 1] = '\0';
    
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##search", "🔍 Search clipboard...", searchBuffer, sizeof(searchBuffer)))
    {
        m_clipboardUIState.searchQuery = searchBuffer;
        m_clipboardManager->SetSearchQuery(m_clipboardUIState.searchQuery);
    }
    
    ImGui::EndChild();
    
    // Control buttons
    ImGui::SameLine();
    
    float buttonSpacing = 8.0f;
    float buttonWidth = 60.0f;
    
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    
    // Format filter dropdown
    const char* formatNames[] = { "All", "Text", "Rich Text", "Images", "Files" };
    int currentFormat = static_cast<int>(m_clipboardUIState.filterFormat);
    
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::Combo("##filter", &currentFormat, formatNames, IM_ARRAYSIZE(formatNames)))
    {
        m_clipboardUIState.filterFormat = static_cast<Clipboard::ClipboardFormat>(currentFormat);
        m_clipboardManager->SetFormatFilter(m_clipboardUIState.filterFormat);
    }
    
    ImGui::SameLine(0, buttonSpacing);
    
    // Show favorites toggle
    bool showFavorites = m_clipboardUIState.showFavorites;
    if (showFavorites)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.2f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.5f, 0.1f, 1.0f));
    }
    
    if (ImGui::Button("⭐ Fav", ImVec2(buttonWidth, 28.0f)))
    {
        m_clipboardUIState.showFavorites = !m_clipboardUIState.showFavorites;
    }
    
    if (showFavorites)
    {
        ImGui::PopStyleColor(3);
    }
    
    ImGui::SameLine(0, buttonSpacing);
    
    // Clear history button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
    
    if (ImGui::Button("🗑️ Clear", ImVec2(buttonWidth, 28.0f)))
    {
        ClearClipboardHistory();
    }
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine(0, buttonSpacing);
    
    // Settings button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    
    if (ImGui::Button("⚙️", ImVec2(28.0f, 28.0f)))
    {
        m_showClipboardSettings = true;
    }
    ImGui::PopStyleColor(3);
    
    ImGui::PopStyleVar(); // FrameRounding
}

void MainWindow::RenderClipboardList()
{
    auto history = m_clipboardUIState.showFavorites ? 
                   m_clipboardManager->GetFavorites() : 
                   m_clipboardManager->GetHistory();
    
    if (history.empty())
    {
        ImVec2 centerPos = ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 80, 100);
        ImGui::SetCursorPos(centerPos);
        
        if (m_clipboardUIState.showFavorites)
        {
            ImGui::Text("No favorite items");
            ImGui::SetCursorPos(ImVec2(centerPos.x - 20, centerPos.y + 20));
            ImGui::Text("Mark items as favorites to see them here");
        }
        else if (!m_clipboardUIState.searchQuery.empty())
        {
            ImGui::Text("No items match your search");
            ImGui::SetCursorPos(ImVec2(centerPos.x - 30, centerPos.y + 20));
            ImGui::Text("Try a different search term");
        }
        else
        {
            ImGui::Text("Clipboard history is empty");
            ImGui::SetCursorPos(ImVec2(centerPos.x - 25, centerPos.y + 20));
            ImGui::Text("Copy something to get started!");
        }
        return;
    }
    
    // Render items
    for (size_t i = 0; i < history.size(); ++i)
    {
        auto item = history[i];
        bool isSelected = (static_cast<int>(i) == m_clipboardUIState.selectedItemIndex);
        
        if (i > 0)
        {
            ImGui::Spacing();
        }
        
        RenderClipboardItem(item, static_cast<int>(i), isSelected);
    }
}

void MainWindow::RenderClipboardItem(std::shared_ptr<Clipboard::ClipboardItem> item, int index, bool isSelected)
{
    if (!item) return;
    
    float itemWidth = ImGui::GetContentRegionAvail().x - 8.0f;
    float itemPadding = 8.0f;
    float minItemHeight = 60.0f;
    
    // Item background color
    auto formatColor = GetClipboardFormatColor(item->format);
    ImU32 itemBgColor;
    
    if (isSelected)
    {
        itemBgColor = IM_COL32(
            static_cast<int>(formatColor.x * 80 + 50),
            static_cast<int>(formatColor.y * 80 + 50),
            static_cast<int>(formatColor.z * 80 + 50),
            255
        );
    }
    else
    {
        itemBgColor = IM_COL32(
            static_cast<int>(formatColor.x * 40 + 30),
            static_cast<int>(formatColor.y * 40 + 30),
            static_cast<int>(formatColor.z * 40 + 30),
            255
        );
    }
    
    ImU32 itemBorderColor = IM_COL32(
        static_cast<int>(formatColor.x * 120 + 60),
        static_cast<int>(formatColor.y * 120 + 60),
        static_cast<int>(formatColor.z * 120 + 60),
        255
    );
    
    // Calculate item height
    float lineHeight = ImGui::GetTextLineHeight();
    float itemHeight = itemPadding * 2 + lineHeight; // Title
    itemHeight += lineHeight + 4; // Preview
    itemHeight += lineHeight + 4; // Metadata
    
    if (itemHeight < minItemHeight)
        itemHeight = minItemHeight;
    
    // Draw item background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 cursorPos = ImGui::GetCursorPos();
    
    ImVec2 itemMin = ImVec2(windowPos.x + cursorPos.x, windowPos.y + cursorPos.y);
    ImVec2 itemMax = ImVec2(itemMin.x + itemWidth, itemMin.y + itemHeight);
    
    // Item shadow
    ImVec2 shadowOffset = ImVec2(2, 2);
    drawList->AddRectFilled(
        ImVec2(itemMin.x + shadowOffset.x, itemMin.y + shadowOffset.y),
        ImVec2(itemMax.x + shadowOffset.x, itemMax.y + shadowOffset.y),
        IM_COL32(0, 0, 0, 30), 6.0f
    );
    
    // Item background and border
    drawList->AddRectFilled(itemMin, itemMax, itemBgColor, 6.0f);
    drawList->AddRect(itemMin, itemMax, itemBorderColor, 6.0f, 0, isSelected ? 2.0f : 1.0f);
    
    // Begin item content
    ImGui::PushID(item->id.c_str());
    
    // Position cursor for content
    ImGui::SetCursorPos(ImVec2(cursorPos.x + itemPadding, cursorPos.y + itemPadding));
    
    // Item title with format icon
    std::string formatIcon = "📄"; // Default text icon
    switch (item->format)
    {
        case Clipboard::ClipboardFormat::Text: formatIcon = "📄"; break;
        case Clipboard::ClipboardFormat::RichText: formatIcon = "📝"; break;
        case Clipboard::ClipboardFormat::Image: formatIcon = "🖼️"; break;
        case Clipboard::ClipboardFormat::Files: formatIcon = "📁"; break;
        default: formatIcon = "❓"; break;
    }
    
    ImGui::Text("%s %s", formatIcon.c_str(), item->title.c_str());
    
    // Favorite and pin indicators
    ImGui::SameLine();
    if (item->isFavorite)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "⭐");
    }
    if (item->isPinned)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "📌");
    }
    
    // Item preview
    if (!item->preview.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::TextWrapped("%s", item->preview.c_str());
        ImGui::PopStyleColor();
    }
    
    // Item metadata (timestamp, size, source)
    ImGui::SetCursorPos(ImVec2(cursorPos.x + itemPadding, 
                              itemMin.y + itemHeight - windowPos.y - lineHeight - itemPadding));
    
    std::string timestamp = FormatClipboardTimestamp(item->timestamp);
    std::string metadata = timestamp + " • " + item->GetSizeString();
    if (!item->source.empty())
    {
        metadata += " • " + item->source;
    }
    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::Text("%s", metadata.c_str());
    ImGui::PopStyleColor();
    
    // Position cursor after item
    ImGui::SetCursorPos(ImVec2(cursorPos.x, cursorPos.y + itemHeight + 4));
    
    // Invisible button for selection
    ImGui::SetCursorPos(ImVec2(cursorPos.x, cursorPos.y));
    bool itemClicked = ImGui::InvisibleButton("##itembutton", ImVec2(itemWidth, itemHeight));
    
    // Handle selection
    if (itemClicked)
    {
        m_clipboardUIState.selectedItemIndex = index;
    }
    
    // Double-click to copy
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        CopyClipboardItem(item);
    }
    
    // Context menu
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("📄 Copy to Clipboard"))
        {
            CopyClipboardItem(item);
        }
        
        ImGui::Separator();
        
        if (ImGui::MenuItem(item->isFavorite ? "⭐ Remove from Favorites" : "⭐ Add to Favorites"))
        {
            ToggleClipboardFavorite(item->id);
        }
        
        if (ImGui::MenuItem(item->isPinned ? "📌 Unpin" : "📌 Pin"))
        {
            m_clipboardManager->TogglePin(item->id);
        }
        
        ImGui::Separator();
        
        if (ImGui::MenuItem("🗑️ Delete", nullptr, false, true))
        {
            DeleteClipboardItem(item->id);
        }
        
        ImGui::EndPopup();
    }
    
    ImGui::PopID();
}

void MainWindow::RenderClipboardPreview()
{
    auto history = m_clipboardUIState.showFavorites ? 
                   m_clipboardManager->GetFavorites() : 
                   m_clipboardManager->GetHistory();
    
    if (m_clipboardUIState.selectedItemIndex < 0 || 
        m_clipboardUIState.selectedItemIndex >= static_cast<int>(history.size()))
    {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No item selected");
        ImGui::Text("Select an item to preview its content");
        return;
    }
    
    auto item = history[m_clipboardUIState.selectedItemIndex];
    if (!item) return;
    
    // Preview header
    ImGui::Text("Preview: %s", item->title.c_str());
    ImGui::Separator();
    ImGui::Spacing();
    
    // Item details
    ImGui::Text("Type: %s", GetClipboardFormatName(item->format));
    ImGui::Text("Size: %s", item->GetSizeString().c_str());
    ImGui::Text("Time: %s", FormatClipboardTimestamp(item->timestamp).c_str());
    if (!item->source.empty())
    {
        ImGui::Text("Source: %s", item->source.c_str());
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Content preview
    switch (item->format)
    {
        case Clipboard::ClipboardFormat::Text:
        case Clipboard::ClipboardFormat::RichText:
            ImGui::Text("Content:");
            ImGui::BeginChild("##TextPreview", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            ImGui::TextWrapped("%s", item->content.c_str());
            ImGui::EndChild();
            break;
            
        case Clipboard::ClipboardFormat::Files:
            ImGui::Text("Files:");
            for (const auto& filePath : item->filePaths)
            {
                ImGui::BulletText("%s", filePath.c_str());
            }
            break;
            
        case Clipboard::ClipboardFormat::Image:
            ImGui::Text("Image:");
            ImGui::Text("Image preview not yet implemented");
            // TODO: Render image preview
            break;
            
        default:
            ImGui::Text("Preview not available for this content type");
            break;
    }
}

// Clipboard helper methods
void MainWindow::OnClipboardItemAdded(std::shared_ptr<Clipboard::ClipboardItem> item)
{
    if (item)
    {
        Logger::Debug("Clipboard item added: {}", item->title);
    }
}

void MainWindow::OnClipboardItemDeleted(const std::string& id)
{
    Logger::Debug("Clipboard item deleted: {}", id);
    
    // Reset selection if deleted item was selected
    auto history = m_clipboardUIState.showFavorites ? 
                   m_clipboardManager->GetFavorites() : 
                   m_clipboardManager->GetHistory();
    
    if (m_clipboardUIState.selectedItemIndex >= static_cast<int>(history.size()))
    {
        m_clipboardUIState.selectedItemIndex = -1;
    }
}

void MainWindow::OnClipboardHistoryCleared()
{
    Logger::Debug("Clipboard history cleared");
    m_clipboardUIState.selectedItemIndex = -1;
}

void MainWindow::CopyClipboardItem(std::shared_ptr<Clipboard::ClipboardItem> item)
{
    if (item && m_clipboardManager)
    {
        m_clipboardManager->CopyToClipboard(item);
        Logger::Debug("Copied clipboard item: {}", item->title);
    }
}

void MainWindow::DeleteClipboardItem(const std::string& id)
{
    if (m_clipboardManager)
    {
        m_clipboardManager->DeleteItem(id);
    }
}

void MainWindow::ToggleClipboardFavorite(const std::string& id)
{
    if (m_clipboardManager)
    {
        m_clipboardManager->ToggleFavorite(id);
    }
}

void MainWindow::ClearClipboardHistory()
{
    if (m_clipboardManager)
    {
        m_clipboardManager->ClearHistory();
    }
}

const char* MainWindow::GetClipboardFormatName(Clipboard::ClipboardFormat format) const
{
    switch (format)
    {
        case Clipboard::ClipboardFormat::Text: return "Text";
        case Clipboard::ClipboardFormat::RichText: return "Rich Text";
        case Clipboard::ClipboardFormat::Image: return "Image";
        case Clipboard::ClipboardFormat::Files: return "Files";
        default: return "Unknown";
    }
}

ImVec4 MainWindow::GetClipboardFormatColor(Clipboard::ClipboardFormat format) const
{
    switch (format)
    {
        case Clipboard::ClipboardFormat::Text: return ImVec4(0.5f, 0.8f, 1.0f, 1.0f); // Light blue
        case Clipboard::ClipboardFormat::RichText: return ImVec4(0.8f, 0.6f, 1.0f, 1.0f); // Purple
        case Clipboard::ClipboardFormat::Image: return ImVec4(1.0f, 0.7f, 0.3f, 1.0f); // Orange
        case Clipboard::ClipboardFormat::Files: return ImVec4(0.6f, 1.0f, 0.4f, 1.0f); // Green
        default: return ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray
    }
}

std::string MainWindow::FormatClipboardTimestamp(const std::chrono::system_clock::time_point& timestamp) const
{
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - timestamp);
    
    if (diff.count() < 60)
    {
        return "Just now";
    }
    else if (diff.count() < 3600)
    {
        int minutes = static_cast<int>(diff.count() / 60);
        return std::to_string(minutes) + "m ago";
    }
    else if (diff.count() < 86400)
    {
        int hours = static_cast<int>(diff.count() / 3600);
        return std::to_string(hours) + "h ago";
    }
    else
    {
        int days = static_cast<int>(diff.count() / 86400);
        return std::to_string(days) + "d ago";
    }
}

// =============================================================================
// KANBAN MODULE IMPLEMENTATION
// =============================================================================
void MainWindow::RenderFileConverterModule()
{
    if (!m_fileConverter)
    {
        ImGui::Text("File Converter not available");
        return;
    }
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    
    // Header with controls
    RenderFileConverterHeader();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Main content area
    float leftPanelWidth = ImGui::GetContentRegionAvail().x * 0.65f;
    float rightPanelWidth = ImGui::GetContentRegionAvail().x * 0.35f - 10.0f;
    
    // Left panel: Drop zone and job queue
    ImGui::BeginChild("##FileConverterLeft", ImVec2(leftPanelWidth, 0), false);
    
    // Drop zone
    RenderFileConverterDropZone();
    
    ImGui::Spacing();
    
    // Job queue
    RenderFileConverterJobQueue();
    
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Right panel: Settings and progress
    ImGui::BeginChild("##FileConverterRight", ImVec2(rightPanelWidth, 0), true);
    
    // Settings
    RenderFileConverterSettings();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Progress and stats
    RenderFileConverterProgress();
    
    ImGui::Spacing();
    
    RenderFileConverterStats();
    
    ImGui::EndChild();
    
    ImGui::PopStyleVar();
}

void MainWindow::RenderFileConverterHeader()
{
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float buttonAreaWidth = 300.0f;
    float titleAreaWidth = availableWidth - buttonAreaWidth - 20.0f;
    
    // Title area
    ImGui::BeginChild("##FileConverterHeaderTitle", ImVec2(titleAreaWidth, 30.0f), false, 
                     ImGuiWindowFlags_NoScrollbar);
    
    ImGui::Text("🔄 File Converter & Compressor");
    
    // Stats summary
    int totalJobs = static_cast<int>(m_fileConverter->GetJobs().size());
    int completedJobs = m_fileConverter->GetCompletedJobCount();
    int failedJobs = m_fileConverter->GetFailedJobCount();
    
    if (totalJobs > 0)
    {
        ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
        ImGui::Text("Jobs:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%d", totalJobs);
        
        if (completedJobs > 0)
        {
            ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
            ImGui::Text("Completed:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "%d", completedJobs);
        }
        
        if (failedJobs > 0)
        {
            ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
            ImGui::Text("Failed:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%d", failedJobs);
        }
    }
    
    ImGui::EndChild();
    
    // Control buttons
    ImGui::SameLine();
    
    float buttonSpacing = 8.0f;
    float buttonWidth = 80.0f;
    
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    
    // Process All button
    bool hasUnprocessedJobs = false;
    for (const auto& job : m_fileConverter->GetJobs())
    {
        if (!job->isCompleted)
        {
            hasUnprocessedJobs = true;
            break;
        }
    }
    
    if (!hasUnprocessedJobs) ImGui::BeginDisabled();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
    
    if (ImGui::Button("▶ Process", ImVec2(buttonWidth, 28.0f)))
    {
        m_fileConverter->ProcessAllJobs();
    }
    ImGui::PopStyleColor(3);
    
    if (!hasUnprocessedJobs) ImGui::EndDisabled();
    
    ImGui::SameLine(0, buttonSpacing);
    
    // Clear Completed button
    bool hasCompletedJobs = completedJobs > 0;
    if (!hasCompletedJobs) ImGui::BeginDisabled();
    
    if (ImGui::Button("🗑️ Clear", ImVec2(buttonWidth, 28.0f)))
    {
        m_fileConverter->ClearCompleted();
    }
    
    if (!hasCompletedJobs) ImGui::EndDisabled();
    
    ImGui::SameLine(0, buttonSpacing);
    
    // Settings toggle
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    
    if (ImGui::Button("⚙️", ImVec2(28.0f, 28.0f)))
    {
        m_showFileConverterSettings = !m_showFileConverterSettings;
    }
    ImGui::PopStyleColor(3);
    
    ImGui::PopStyleVar(); // FrameRounding
}

void MainWindow::RenderFileConverterDropZone()
{
    ImVec2 dropZoneSize = ImVec2(ImGui::GetContentRegionAvail().x - 16, 120);
    ImVec2 dropZonePos = ImGui::GetCursorPos();
    
    // Drop zone background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 dropZoneMin = ImVec2(windowPos.x + dropZonePos.x + 8, windowPos.y + dropZonePos.y);
    ImVec2 dropZoneMax = ImVec2(dropZoneMin.x + dropZoneSize.x, dropZoneMin.y + dropZoneSize.y);
    
    // Background color based on drag state
    ImU32 bgColor = m_fileConverterUIState.isDragOver ? 
                    IM_COL32(40, 60, 40, 255) : IM_COL32(25, 25, 25, 255);
    ImU32 borderColor = m_fileConverterUIState.isDragOver ? 
                        IM_COL32(100, 200, 100, 255) : IM_COL32(100, 100, 100, 255);
    
    // Draw drop zone
    drawList->AddRectFilled(dropZoneMin, dropZoneMax, bgColor, 6.0f);
    drawList->AddRect(dropZoneMin, dropZoneMax, borderColor, 6.0f, 0, 2.0f);
    
    // Drop zone content
    ImGui::SetCursorPos(ImVec2(dropZonePos.x + dropZoneSize.x * 0.5f - 120, dropZonePos.y + 35));
    if (m_fileConverterUIState.isDragOver)
    {
        ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.7f, 1.0f), "🎯 Drop files here to convert");
    }
    else
    {
        ImGui::Text("📁 Drop images (PNG, JPG) or PDFs here");
    }
    
    ImGui::SetCursorPos(ImVec2(dropZonePos.x + dropZoneSize.x * 0.5f - 100, dropZonePos.y + 60));
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Supports: PNG, JPG, PDF compression & conversion");
    
    // Add Browse button
    ImGui::SetCursorPos(ImVec2(dropZonePos.x + dropZoneSize.x * 0.5f - 40, dropZonePos.y + 85));
    if (ImGui::Button("📂 Browse", ImVec2(80, 25)))
    {
        // TODO: Implement file browser dialog
        Logger::Info("Browse button clicked - implement file dialog");
    }
    
    // Move cursor past drop zone
    ImGui::SetCursorPos(ImVec2(dropZonePos.x, dropZonePos.y + dropZoneSize.y + 16));
    
    // Invisible button for drag & drop detection
    ImGui::SetCursorPos(dropZonePos);
    ImGui::InvisibleButton("##dropzone", dropZoneSize);
    
    // Handle drag & drop (simplified - you'd need proper Windows drag/drop integration)
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILES"))
        {
            // Handle dropped files
            m_fileConverterUIState.isDragOver = false;
            // TODO: Process dropped files
        }
        else
        {
            m_fileConverterUIState.isDragOver = true;
        }
        ImGui::EndDragDropTarget();
    }
    else
    {
        m_fileConverterUIState.isDragOver = false;
    }
}

void MainWindow::RenderFileConverterSettings()
{
    ImGui::Text("⚙️ Conversion Settings");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Output format selection
    ImGui::Text("Output Format:");
    const char* formatNames[] = { "Keep Original", "PNG", "JPG", "PDF" };
    int currentFormat = static_cast<int>(m_fileConverterUIState.outputFormat);
    if (ImGui::Combo("##outputformat", &currentFormat, formatNames, IM_ARRAYSIZE(formatNames)))
    {
        m_fileConverterUIState.outputFormat = static_cast<FileType>(currentFormat);
    }
    
    ImGui::Spacing();
    
    // Quality settings for images
    if (m_fileConverterUIState.outputFormat == FileType::JPG || 
        m_fileConverterUIState.outputFormat == FileType::Unknown)
    {
        ImGui::Text("JPEG Quality:");
        ImGui::SliderInt("##jpegquality", &m_fileConverterUIState.imageQuality, 1, 100, "%d%%");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Higher values = better quality, larger file size");
        }
    }
    
    if (m_fileConverterUIState.outputFormat == FileType::PNG || 
        m_fileConverterUIState.outputFormat == FileType::Unknown)
    {
        ImGui::Text("PNG Compression:");
        ImGui::SliderInt("##pngcompression", &m_fileConverterUIState.pngCompression, 0, 9, "Level %d");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Higher values = smaller file size, slower compression");
        }
    }
    
    ImGui::Spacing();
    
    // Target file size
    ImGui::Text("Target Size (KB):");
    int targetSize = static_cast<int>(m_fileConverterUIState.targetSizeKB);
    if (ImGui::InputInt("##targetsize", &targetSize, 10, 100))
    {
        m_fileConverterUIState.targetSizeKB = (targetSize > 0) ? targetSize : 0;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("0 = no size limit, >0 = target file size in KB");
    }
    
    ImGui::Spacing();
    
    // Additional options
    ImGui::Checkbox("Preserve Metadata", &m_fileConverterUIState.preserveMetadata);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Keep EXIF data, color profiles, etc.");
    }
    
    ImGui::Checkbox("Auto-process new jobs", &m_fileConverterUIState.autoProcessJobs);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Automatically start processing when files are added");
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Output directory
    ImGui::Text("📁 Output Directory:");
    ImGui::Checkbox("Use source directory", &m_fileConverterUIState.useSourceDirectory);
    
    if (!m_fileConverterUIState.useSourceDirectory)
    {
        char outputDirBuffer[512];
        strncpy_s(outputDirBuffer, m_fileConverterUIState.outputDirectory.c_str(), sizeof(outputDirBuffer) - 1);
        outputDirBuffer[sizeof(outputDirBuffer) - 1] = '\0';
        
        ImGui::InputText("##outputdir", outputDirBuffer, sizeof(outputDirBuffer));
        m_fileConverterUIState.outputDirectory = outputDirBuffer;
        
        ImGui::SameLine();
        if (ImGui::Button("📂"))
        {
            // TODO: Open folder selection dialog
            Logger::Info("Output directory browse clicked");
        }
    }
}

void MainWindow::RenderFileConverterJobQueue()
{
    ImGui::Text("📋 Conversion Queue");
    ImGui::Separator();
    ImGui::Spacing();
    
    auto jobs = m_fileConverter->GetJobs();
    
    if (jobs.empty())
    {
        ImVec2 centerPos = ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 80, 50);
        ImGui::SetCursorPos(centerPos);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No conversion jobs");
        ImGui::SetCursorPos(ImVec2(centerPos.x - 30, centerPos.y + 20));
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Drop files above to start converting");
        return;
    }
    
    // Job list
    ImGui::BeginChild("##JobList", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    
    for (size_t i = 0; i < jobs.size(); ++i)
    {
        auto job = jobs[i];
        bool isSelected = (static_cast<int>(i) == m_fileConverterUIState.selectedJobIndex);
        
        if (i > 0)
        {
            ImGui::Spacing();
        }
        
        RenderFileConverterJob(job, static_cast<int>(i), isSelected);
    }
    
    ImGui::EndChild();
}

void MainWindow::RenderFileConverterJob(std::shared_ptr<FileConversionJob> job, int index, bool isSelected)
{
    if (!job) return;
    
    float jobWidth = ImGui::GetContentRegionAvail().x - 8.0f;
    float jobPadding = 8.0f;
    float minJobHeight = 70.0f;
    
    // Job background color based on status
    ImU32 jobBgColor;
    if (job->hasError)
        jobBgColor = IM_COL32(60, 30, 30, 255); // Red for errors
    else if (job->isCompleted)
        jobBgColor = IM_COL32(30, 50, 30, 255); // Green for completed
    else if (job->progress > 0.0f)
        jobBgColor = IM_COL32(40, 40, 60, 255); // Blue for processing
    else
        jobBgColor = IM_COL32(40, 40, 40, 255); // Gray for pending
    
    if (isSelected)
    {
        // Brighten selected job - Use parentheses to avoid Windows macro conflict
        jobBgColor = IM_COL32(
            (std::min)(255, static_cast<int>(((jobBgColor >> 0) & 0xFF) + 20)),
            (std::min)(255, static_cast<int>(((jobBgColor >> 8) & 0xFF) + 20)),
            (std::min)(255, static_cast<int>(((jobBgColor >> 16) & 0xFF) + 20)),
            255
        );
    }
    
    ImU32 jobBorderColor = IM_COL32(80, 80, 80, 255);
    
    // Calculate job height
    float lineHeight = ImGui::GetTextLineHeight();
    float jobHeight = jobPadding * 2 + lineHeight * 3 + 12; // Title, info, progress
    
    if (jobHeight < minJobHeight)
        jobHeight = minJobHeight;
    
    // Draw job background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 cursorPos = ImGui::GetCursorPos();
    
    ImVec2 jobMin = ImVec2(windowPos.x + cursorPos.x, windowPos.y + cursorPos.y);
    ImVec2 jobMax = ImVec2(jobMin.x + jobWidth, jobMin.y + jobHeight);
    
    // Job background and border
    drawList->AddRectFilled(jobMin, jobMax, jobBgColor, 6.0f);
    drawList->AddRect(jobMin, jobMax, jobBorderColor, 6.0f, 0, isSelected ? 2.0f : 1.0f);
    
    // Begin job content
    ImGui::PushID(job->id.c_str());
    
    // Position cursor for content
    ImGui::SetCursorPos(ImVec2(cursorPos.x + jobPadding, cursorPos.y + jobPadding));
    
    // Job title with file type icons
    std::string inputIcon = "📄";
    std::string outputIcon = "📄";
    
    switch (job->inputType)
    {
        case FileType::PNG: inputIcon = "🖼️"; break;
        case FileType::JPG: inputIcon = "🖼️"; break;
        case FileType::PDF: inputIcon = "📕"; break;
    }
    
    switch (job->outputType)
    {
        case FileType::PNG: outputIcon = "🖼️"; break;
        case FileType::JPG: outputIcon = "🖼️"; break;
        case FileType::PDF: outputIcon = "📕"; break;
    }
    
    ImGui::Text("%s %s → %s %s", 
               inputIcon.c_str(), job->GetInputFileName().c_str(),
               outputIcon.c_str(), FileConverter::GetFileTypeString(job->outputType).c_str());
    
    // Status indicator
    ImGui::SameLine();
    if (job->hasError)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "❌ Error");
    }
    else if (job->isCompleted)
    {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "✅ Done");
    }
    else if (job->progress > 0.0f)
    {
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "⏳ Processing");
    }
    else
    {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "⏸️ Pending");
    }
    
    // Job info line
    ImGui::SetCursorPosX(cursorPos.x + jobPadding);
    std::string infoText = job->GetFileSizeString(job->originalSizeBytes);
    if (job->isCompleted && !job->hasError)
    {
        infoText += " → " + job->GetFileSizeString(job->compressedSizeBytes);
        float ratio = job->GetCompressionRatio();
        if (ratio > 0.1f)
        {
            infoText += " (" + std::to_string(static_cast<int>(ratio)) + "% saved)";
        }
    }
    if (job->quality != 85) // Show quality if not default
    {
        infoText += " • Q" + std::to_string(job->quality);
    }
    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::Text("%s", infoText.c_str());
    ImGui::PopStyleColor();
    
    // Progress bar (only if processing or completed)
    if (job->progress > 0.0f || job->isCompleted)
    {
        ImGui::SetCursorPosX(cursorPos.x + jobPadding);
        ImGui::SetNextItemWidth(jobWidth - 2 * jobPadding - 100); // Leave space for time/status
        
        float progress = job->isCompleted ? 1.0f : job->progress;
        
        ImVec4 progressColor;
        if (job->hasError)
            progressColor = ImVec4(1.0f, 0.3f, 0.3f, 0.8f); // Red
        else if (job->isCompleted)
            progressColor = ImVec4(0.3f, 1.0f, 0.3f, 0.8f); // Green
        else
            progressColor = ImVec4(0.3f, 0.6f, 1.0f, 0.8f); // Blue
        
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, progressColor);
        ImGui::ProgressBar(progress, ImVec2(0, 0), "");
        ImGui::PopStyleColor();
        
        // Time/status on the right side of progress bar
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        if (job->hasError)
        {
            ImGui::Text("Failed");
        }
        else if (job->isCompleted)
        {
            ImGui::Text("%s", job->GetProcessingTimeString().c_str());
        }
        else
        {
            ImGui::Text("%.0f%%", progress * 100.0f);
        }
        ImGui::PopStyleColor();
    }
    
    // Position cursor after job
    ImGui::SetCursorPos(ImVec2(cursorPos.x, cursorPos.y + jobHeight + 4));
    
    // Invisible button for selection
    ImGui::SetCursorPos(ImVec2(cursorPos.x, cursorPos.y));
    bool jobClicked = ImGui::InvisibleButton("##jobbutton", ImVec2(jobWidth, jobHeight));
    
    // Handle selection
    if (jobClicked)
    {
        m_fileConverterUIState.selectedJobIndex = index;
    }
    
    // Context menu
    if (ImGui::BeginPopupContextItem())
    {
        if (!job->isCompleted && ImGui::MenuItem("▶️ Process Now"))
        {
            m_fileConverter->ProcessJob(job->id);
        }
        
        if (job->isCompleted && !job->hasError && ImGui::MenuItem("📂 Show Output"))
        {
            // TODO: Open file explorer to output file
            Logger::Info("Show output file: {}", job->outputPath);
        }
        
        if (job->hasError && ImGui::MenuItem("🔄 Retry"))
        {
            job->hasError = false;
            job->isCompleted = false;
            job->progress = 0.0f;
            job->errorMessage.clear();
        }
        
        ImGui::Separator();
        
        if (ImGui::MenuItem("🗑️ Remove"))
        {
            m_fileConverter->RemoveJob(job->id);
            if (m_fileConverterUIState.selectedJobIndex == index)
            {
                m_fileConverterUIState.selectedJobIndex = -1;
            }
        }
        
        ImGui::EndPopup();
    }
    
    ImGui::PopID();
}

void MainWindow::RenderFileConverterProgress()
{
    ImGui::Text("📊 Progress");
    ImGui::Separator();
    ImGui::Spacing();
    
    bool isProcessing = m_fileConverter->IsProcessing();
    
    if (isProcessing)
    {
        ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "⏳ Processing...");
        
        // Find current processing job
        for (const auto& job : m_fileConverter->GetJobs())
        {
            if (!job->isCompleted && job->progress > 0.0f)
            {
                ImGui::Text("Current: %s", job->GetInputFileName().c_str());
                ImGui::ProgressBar(job->progress, ImVec2(-1, 0), "");
                break;
            }
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "💤 Idle");
    }
}

void MainWindow::RenderFileConverterStats()
{
    ImGui::Text("📈 Statistics");
    ImGui::Separator();
    ImGui::Spacing();
    
    auto jobs = m_fileConverter->GetJobs();
    int total = static_cast<int>(jobs.size());
    int completed = m_fileConverter->GetCompletedJobCount();
    int failed = m_fileConverter->GetFailedJobCount();
    int pending = total - completed - failed;
    
    ImGui::Text("Total Jobs: %d", total);
    ImGui::Text("Completed: %d", completed);
    ImGui::Text("Failed: %d", failed);
    ImGui::Text("Pending: %d", pending);
    
    if (completed > 0)
    {
        // Calculate total space saved
        size_t totalOriginal = 0;
        size_t totalCompressed = 0;
        
        for (const auto& job : jobs)
        {
            if (job->isCompleted && !job->hasError)
            {
                totalOriginal += job->originalSizeBytes;
                totalCompressed += job->compressedSizeBytes;
            }
        }
        
        if (totalOriginal > 0)
        {
            float savedRatio = (1.0f - static_cast<float>(totalCompressed) / totalOriginal) * 100.0f;
            size_t savedBytes = totalOriginal - totalCompressed;
            
            ImGui::Spacing();
            ImGui::Text("Space Saved:");
            ImGui::Text("%.1f%% (%s)", savedRatio, FileConversionJob().GetFileSizeString(savedBytes).c_str());
        }
    }
}

// Helper method implementations
void MainWindow::OnFileConverterJobProgress(const std::string& jobId, float progress)
{
    // Update can be handled in real-time through the shared job objects
    // This callback is mainly for logging or triggering UI updates
    Logger::Debug("Job {} progress: {:.1f}%", jobId, progress * 100.0f);
}

void MainWindow::OnFileConverterJobComplete(const std::string& jobId, bool success, const std::string& error)
{
    if (success)
    {
        Logger::Info("Job {} completed successfully", jobId);
    }
    else
    {
        Logger::Error("Job {} failed: {}", jobId, error);
    }
}

void MainWindow::ProcessDroppedFiles(const std::vector<std::string>& files)
{
    for (const auto& filePath : files)
    {
        if (IsFileSupported(filePath))
        {
            AddConversionJob(filePath);
        }
        else
        {
            Logger::Warning("Unsupported file type: {}", filePath);
        }
    }
}

void MainWindow::AddConversionJob(const std::string& inputPath)
{
    FileType inputType = FileConverter::GetFileTypeFromPath(inputPath);
    FileType outputType = m_fileConverterUIState.outputFormat;
    
    // If output format is "Keep Original", use same type for compression
    if (outputType == FileType::Unknown)
    {
        outputType = inputType;
    }
    
    std::string outputPath = GenerateOutputPath(inputPath, outputType);
    
    // Create job with current settings
    FileConversionJob jobSettings;
    jobSettings.quality = (outputType == FileType::JPG) ? 
                         m_fileConverterUIState.imageQuality : 
                         m_fileConverterUIState.pngCompression;
    jobSettings.targetSizeKB = m_fileConverterUIState.targetSizeKB;
    jobSettings.preserveMetadata = m_fileConverterUIState.preserveMetadata;
    
    std::string jobId = m_fileConverter->AddConversionJob(inputPath, outputPath, outputType, jobSettings);
    
    // Auto-process if enabled
    if (m_fileConverterUIState.autoProcessJobs)
    {
        m_fileConverter->ProcessJob(jobId);
    }
    
    Logger::Info("Added conversion job: {} -> {}", inputPath, outputPath);
}

std::string MainWindow::GenerateOutputPath(const std::string& inputPath, FileType outputType)
{
    namespace fs = std::filesystem;

    fs::path input(inputPath);
    fs::path outputDir;
    
    if (m_fileConverterUIState.useSourceDirectory || m_fileConverterUIState.outputDirectory.empty())
    {
        outputDir = input.parent_path();
    }
    else
    {
        outputDir = m_fileConverterUIState.outputDirectory;
    }
    
    // Generate output filename
    std::string stem = input.stem().string();
    std::string extension;
    
    switch (outputType)
    {
        case FileType::PNG: extension = ".png"; break;
        case FileType::JPG: extension = ".jpg"; break;
        case FileType::PDF: extension = ".pdf"; break;
        default: extension = input.extension().string(); break;
    }
    
    // Add suffix to avoid overwriting
    // Add suffix to avoid overwriting
    std::string outputFilename = stem + "_converted" + extension;
    fs::path outputPath = outputDir / outputFilename;
    
    // Make sure we don't overwrite existing files
    int counter = 1;
    while (fs::exists(outputPath))
    {
        outputFilename = stem + "_converted_" + std::to_string(counter) + extension;
        outputPath = outputDir / outputFilename;
        counter++;
    }
    
    return outputPath.string();
}

bool MainWindow::IsFileSupported(const std::string& path)
{
    FileType type = FileConverter::GetFileTypeFromPath(path);
    return type != FileType::Unknown;
}

// =============================================================================
// KANBAN MODULE IMPLEMENTATION
// =============================================================================
void MainWindow::RenderSettingsModule()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    // Main settings area with sidebar
    float sidebarWidth = 200.0f;
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float contentWidth = availableWidth - sidebarWidth - 10.0f;
    
    // Settings sidebar
    ImGui::BeginChild("##SettingsSidebar", ImVec2(sidebarWidth, 0), true);
    RenderSettingsSidebar();
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Settings content area
    ImGui::BeginChild("##SettingsContent", ImVec2(contentWidth, 0), false);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
    RenderSettingsContent();
    ImGui::PopStyleVar();
    ImGui::EndChild();
    
    // Modal dialogs
    RenderResetConfirmDialog();
    RenderDataManagementDialogs();
    
    ImGui::PopStyleVar();
}

void MainWindow::RenderSettingsSidebar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    
    ImGui::Text("⚙️ Settings");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Settings categories
    const SettingsCategory categories[] = {
        SettingsCategory::General,
        SettingsCategory::Appearance,
        SettingsCategory::Hotkeys,
        SettingsCategory::Modules,
        SettingsCategory::Account,
        SettingsCategory::About
    };
    
    const char* icons[] = {
        "🔧", "🎨", "⌨️", "📦", "👤", "ℹ️"
    };
    
    for (int i = 0; i < 6; ++i)
    {
        SettingsCategory category = categories[i];
        bool isSelected = (m_settingsUIState.selectedCategory == category);
        
        if (isSelected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.6f, 0.9f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.4f, 0.7f, 1.0f));
        }
        
        std::string buttonText = std::string(icons[i]) + " " + GetCategoryName(category);
        
        if (ImGui::Button(buttonText.c_str(), ImVec2(-1, 35)))
        {
            m_settingsUIState.selectedCategory = category;
        }
        
        if (isSelected)
        {
            ImGui::PopStyleColor(3);
        }
        
        if (i < 5) ImGui::Spacing();
    }
    
    ImGui::PopStyleVar();
}

void MainWindow::RenderSettingsContent()
{
    switch (m_settingsUIState.selectedCategory)
    {
        case SettingsCategory::General:
            RenderGeneralSettings();
            break;
        case SettingsCategory::Appearance:
            RenderAppearanceSettings();
            break;
        case SettingsCategory::Hotkeys:
            RenderHotkeySettings();
            break;
        case SettingsCategory::Modules:
            RenderModuleSettings();
            break;
        case SettingsCategory::Account:
            RenderAccountSettings();
            break;
        case SettingsCategory::About:
            RenderAboutSettings();
            break;
    }
}

void MainWindow::RenderGeneralSettings()
{
    ImGui::Text("🔧 General Settings");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Startup behavior
    ImGui::Text("Startup Behavior");
    ImGui::Spacing();
    
    if (ImGui::Checkbox("Start with Windows", &m_settingsUIState.startWithWindows))
    {
        // TODO: Implement Windows startup registration
        Logger::Info("Start with Windows toggled: {}", m_settingsUIState.startWithWindows);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Automatically start Potensio when Windows starts");
    
    ImGui::Checkbox("Start minimized", &m_settingsUIState.startMinimized);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Start the application in the system tray");
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Window behavior
    ImGui::Text("Window Behavior");
    ImGui::Spacing();
    
    ImGui::Checkbox("Minimize to system tray", &m_settingsUIState.minimizeToTray);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Hide to system tray instead of taskbar when minimized");
    
    ImGui::Checkbox("Close to system tray", &m_settingsUIState.closeToTray);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Keep running in system tray when window is closed");
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Notifications and sounds
    ImGui::Text("Notifications & Sounds");
    ImGui::Spacing();
    
    ImGui::Checkbox("Show notifications", &m_settingsUIState.showNotifications);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Show system notifications for events");
    
    ImGui::Checkbox("Enable sounds", &m_settingsUIState.enableSounds);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Play sound effects for actions and notifications");
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Data management
    ImGui::Text("Data Management");
    ImGui::Spacing();
    
    ImGui::Checkbox("Auto-save data", &m_settingsUIState.autoSave);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Automatically save changes");
    
    if (m_settingsUIState.autoSave)
    {
        ImGui::Text("Auto-save interval:");
        ImGui::SliderInt("##autosave", &m_settingsUIState.autoSaveInterval, 5, 300, "%d seconds");
    }
    
    ImGui::Spacing();
    
    // Data management buttons
    if (ImGui::Button("📤 Export Data", ImVec2(120, 0)))
    {
        m_showExportDataDialog = true;
    }
    ImGui::SameLine();
    
    if (ImGui::Button("📥 Import Data", ImVec2(120, 0)))
    {
        m_showImportDataDialog = true;
    }
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
    if (ImGui::Button("🗑️ Clear All Data", ImVec2(120, 0)))
    {
        m_showClearDataDialog = true;
    }
    ImGui::PopStyleColor(2);
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Reset settings
    ImGui::Text("Reset");
    ImGui::Spacing();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    if (ImGui::Button("🔄 Reset to Defaults", ImVec2(150, 0)))
    {
        m_showResetConfirmDialog = true;
    }
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Reset all settings to their default values");
}

void MainWindow::RenderAppearanceSettings()
{
    ImGui::Text("🎨 Appearance Settings");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Theme selection
    ImGui::Text("Theme");
    ImGui::Spacing();
    
    const char* themeNames[] = { "Dark", "Light", "Auto (System)" };
    int currentTheme = m_settingsUIState.themeMode;
    
    if (ImGui::Combo("Theme Mode", &currentTheme, themeNames, IM_ARRAYSIZE(themeNames)))
    {
        m_settingsUIState.themeMode = currentTheme;
        ApplyThemeSettings();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Choose between dark, light, or system-matching theme");
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // UI Scale
    ImGui::Text("User Interface");
    ImGui::Spacing();
    
    float currentScale = m_settingsUIState.uiScale;
    if (ImGui::SliderFloat("UI Scale", &currentScale, 0.8f, 2.0f, "%.1f"))
    {
        m_settingsUIState.uiScale = currentScale;
        // TODO: Apply UI scale changes
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Adjust the size of UI elements");
    
    ImGui::Checkbox("Compact mode", &m_settingsUIState.compactMode);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Use smaller spacing and elements to fit more content");
    
    ImGui::Checkbox("Enable animations", &m_settingsUIState.enableAnimations);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Enable smooth transitions and animations");
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Accent colors
    ImGui::Text("Accent Color");
    ImGui::Spacing();
    
    const char* colorNames[] = { 
        "Blue", "Green", "Purple", "Orange", "Red", "Teal", "Pink", "Yellow" 
    };
    
    if (ImGui::Combo("Accent Color", &m_settingsUIState.accentColor, colorNames, IM_ARRAYSIZE(colorNames)))
    {
        // TODO: Apply accent color changes
    }
    
    // Color preview
    ImVec4 accentColor = GetAccentColor(m_settingsUIState.accentColor);
    ImGui::SameLine();
    ImGui::ColorButton("##accent_preview", accentColor, ImGuiColorEditFlags_NoTooltip, ImVec2(30, 30));
    
    ImGui::Spacing();
    
    // Preview area
    ImGui::Text("Preview");
    ImGui::BeginChild("##ThemePreview", ImVec2(0, 120), true);
    
    ImGui::Text("Sample text in current theme");
    ImGui::Button("Sample Button");
    ImGui::SameLine();
    ImGui::ProgressBar(0.65f, ImVec2(-1, 0), "Sample Progress");
    
    ImGui::Text("🎯 Accent color elements");
    ImGui::PushStyleColor(ImGuiCol_Button, accentColor);
    ImGui::Button("Accent Button");
    ImGui::PopStyleColor();
    
    ImGui::EndChild();
}

void MainWindow::RenderHotkeySettings()
{
    ImGui::Text("⌨️ Hotkey Settings");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.5f, 1.0f), 
                      "Click on a hotkey field and press the desired key combination");
    ImGui::Spacing();
    
    // Global hotkeys
    ImGui::Text("Global Hotkeys");
    ImGui::Spacing();
    
    RenderHotkeyEditor("Show/Hide Potensio", m_settingsUIState.globalHotkeyBuffer, 
                      sizeof(m_settingsUIState.globalHotkeyBuffer));
    
    RenderHotkeyEditor("Quick Capture", m_settingsUIState.quickCaptureBuffer, 
                      sizeof(m_settingsUIState.quickCaptureBuffer));
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Module hotkeys
    ImGui::Text("Module Hotkeys");
    ImGui::Spacing();
    
    RenderHotkeyEditor("Start/Stop Pomodoro", m_settingsUIState.pomodoroStartBuffer, 
                      sizeof(m_settingsUIState.pomodoroStartBuffer));
    
    RenderHotkeyEditor("Show Today's Tasks", m_settingsUIState.showTodayTasksBuffer, 
                      sizeof(m_settingsUIState.showTodayTasksBuffer));
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Hotkey help
    ImGui::Text("📋 Hotkey Tips");
    ImGui::BulletText("Use Ctrl, Alt, Shift modifiers");
    ImGui::BulletText("Function keys (F1-F12) work well");
    ImGui::BulletText("Avoid system-reserved combinations");
    ImGui::BulletText("Leave empty to disable a hotkey");
    
    ImGui::Spacing();
    
    if (ImGui::Button("🔄 Reset Hotkeys", ImVec2(120, 0)))
    {
        // Reset to defaults
        strcpy_s(m_settingsUIState.globalHotkeyBuffer, "Ctrl+Shift+P");
        strcpy_s(m_settingsUIState.pomodoroStartBuffer, "Ctrl+Alt+P");
        strcpy_s(m_settingsUIState.quickCaptureBuffer, "Ctrl+Shift+C");
        strcpy_s(m_settingsUIState.showTodayTasksBuffer, "Ctrl+Shift+T");
    }
}

void MainWindow::RenderModuleSettings()
{
    ImGui::Text("📦 Module Settings");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Text("Enable/Disable Modules");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                      "Disabled modules will be hidden from the sidebar");
    ImGui::Spacing();
    
    // Module toggles with descriptions
    struct ModuleInfo {
        bool* enabled;
        const char* name;
        const char* icon;
        const char* description;
    };
    
    ModuleInfo modules[] = {
        { &m_settingsUIState.enableDropover, "Dropover", "📁", "File staging and batch operations" },
        { &m_settingsUIState.enablePomodoro, "Pomodoro Timer", "🍅", "Time management and productivity tracking" },
        { &m_settingsUIState.enableKanban, "Kanban Board", "📋", "Project management and task tracking" },
        { &m_settingsUIState.enableTodo, "Todo List", "✅", "Daily task management and scheduling" },
        { &m_settingsUIState.enableClipboard, "Clipboard Manager", "📄", "Clipboard history and management" },
        { &m_settingsUIState.enableFileConverter, "File Converter", "🔄", "File format conversion and compression" }
    };
    
    for (const auto& module : modules)
    {
        ImGui::PushID(module.name);
        
        // Module header
        if (ImGui::Checkbox((std::string(module.icon) + " " + module.name).c_str(), module.enabled))
        {
            // TODO: Apply module enable/disable logic
            Logger::Info("Module {} {}", module.name, *module.enabled ? "enabled" : "disabled");
        }
        
        // Description
        ImGui::Indent(30);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        ImGui::TextWrapped("%s", module.description);
        ImGui::PopStyleColor();
        ImGui::Unindent(30);
        
        ImGui::Spacing();
        
        ImGui::PopID();
    }
    
    ImGui::Separator();
    ImGui::Spacing();
    
    // Performance settings
    ImGui::Text("Performance");
    ImGui::Spacing();
    
    static bool enableMultithreading = true;
    static bool optimizeMemory = true;
    static int maxHistoryItems = 1000;
    
    ImGui::Checkbox("Enable multithreading", &enableMultithreading);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Use multiple CPU cores for better performance");
    
    ImGui::Checkbox("Optimize memory usage", &optimizeMemory);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Reduce memory usage at the cost of some features");
    
    ImGui::Text("Max history items:");
    ImGui::SliderInt("##maxhistory", &maxHistoryItems, 100, 10000, "%d items");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Maximum number of items to keep in module histories");
}

void MainWindow::RenderAccountSettings()
{
    ImGui::Text("👤 Account Settings");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Future feature notice
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.5f, 1.0f));
    ImGui::Text("🚧 Account features are coming in a future update!");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    
    // Profile section (placeholder)
    ImGui::Text("Profile");
    ImGui::Spacing();
    
    ImGui::BeginDisabled(); // Disable inputs for now
    
    ImGui::Text("Username:");
    ImGui::InputText("##username", m_settingsUIState.usernameBuffer, 
                    sizeof(m_settingsUIState.usernameBuffer));
    
    ImGui::Text("Email:");
    ImGui::InputText("##email", m_settingsUIState.emailBuffer, 
                    sizeof(m_settingsUIState.emailBuffer));
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Sync settings
    ImGui::Text("Synchronization");
    ImGui::Spacing();
    
    ImGui::Checkbox("Enable cloud sync", &m_settingsUIState.syncEnabled);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Sync your data across devices");
    
    ImGui::Checkbox("Enable cloud backup", &m_settingsUIState.cloudBackup);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Backup your data to the cloud");
    
    ImGui::Spacing();
    
    if (ImGui::Button("🔗 Sign In", ImVec2(100, 0)))
    {
        // TODO: Implement sign in
    }
    ImGui::SameLine();
    
    if (ImGui::Button("📝 Create Account", ImVec2(120, 0)))
    {
        // TODO: Implement account creation
    }
    
    ImGui::EndDisabled();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // License info
    ImGui::Text("License");
    ImGui::Spacing();
    
    ImGui::Text("Potensio Personal License");
    ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "✅ Licensed");
    ImGui::Text("Valid until: Not applicable (Lifetime)");
    
    ImGui::Spacing();
    
    if (ImGui::Button("📋 View License", ImVec2(120, 0)))
    {
        // TODO: Show license dialog
    }
}

void MainWindow::RenderAboutSettings()
{
    ImGui::Text("ℹ️ About Potensio");
    ImGui::Separator();
    ImGui::Spacing();
    
    // App info
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use default/larger font if available
    ImGui::Text("Potensio");
    ImGui::PopFont();
    
    ImGui::Text("Version: %s", m_settingsUIState.currentVersion.c_str());
    ImGui::Text("Build Date: %s %s", __DATE__, __TIME__);
    ImGui::Text("Platform: Windows x64");
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Update section
    ImGui::Text("🔄 Updates");
    ImGui::Spacing();
    
    ImGui::Checkbox("Automatically check for updates", &m_settingsUIState.autoCheckUpdates);
    ImGui::Checkbox("Download updates automatically", &m_settingsUIState.downloadUpdatesAuto);
    ImGui::Checkbox("Participate in beta testing", &m_settingsUIState.betaChannel);
    
    ImGui::Spacing();
    
    // Update status
    if (m_settingsUIState.updateAvailable)
    {
        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), 
                          "🎉 Update available: v%s", m_settingsUIState.latestVersion.c_str());
        if (ImGui::Button("📥 Download Update", ImVec2(140, 0)))
        {
            // TODO: Implement update download
        }
    }
    else if (m_settingsUIState.checkingUpdates)
    {
        ImGui::Text("🔍 Checking for updates...");
    }
    else
    {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "✅ You're up to date!");
    }
    
    ImGui::Spacing();
    
    if (ImGui::Button("🔍 Check Now", ImVec2(100, 0)))
    {
        CheckForUpdates();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // System info
    ImGui::Text("💻 System Information");
    ImGui::Spacing();
    
    // Get system info
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    
    ImGui::Text("CPU Cores: %d", sysInfo.dwNumberOfProcessors);
    ImGui::Text("RAM: %.1f GB", memInfo.ullTotalPhys / (1024.0 * 1024.0 * 1024.0));
    
    // OpenGL info
    const char* glVersion = (const char*)glGetString(GL_VERSION);
    const char* glRenderer = (const char*)glGetString(GL_RENDERER);
    if (glVersion) ImGui::Text("OpenGL: %s", glVersion);
    if (glRenderer) ImGui::Text("GPU: %s", glRenderer);
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Credits and links
    ImGui::Text("🙏 Credits & Links");
    ImGui::Spacing();
    
    if (ImGui::Button("🌐 Website", ImVec2(80, 0)))
    {
        // TODO: Open website
    }
    ImGui::SameLine();
    
    if (ImGui::Button("📖 Documentation", ImVec2(120, 0)))
    {
        // TODO: Open documentation
    }
    ImGui::SameLine();
    
    if (ImGui::Button("🐛 Report Bug", ImVec2(100, 0)))
    {
        // TODO: Open bug report
    }
    
    ImGui::Spacing();
    
    ImGui::Text("Built with:");
    ImGui::BulletText("Dear ImGui %s", IMGUI_VERSION);
    ImGui::BulletText("OpenGL");
    ImGui::BulletText("SQLite 3");
    ImGui::BulletText("CMake");
}

// Dialog implementations
void MainWindow::RenderResetConfirmDialog()
{
    if (!m_showResetConfirmDialog) return;
    
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 150), ImGuiCond_Appearing);
    
    if (ImGui::BeginPopupModal("Reset Settings", &m_showResetConfirmDialog, 
                              ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::Text("⚠️ Reset all settings to defaults?");
        ImGui::Spacing();
        ImGui::TextWrapped("This will reset all your preferences, hotkeys, and module settings. This action cannot be undone.");
        ImGui::Spacing();
        
        if (ImGui::Button("🔄 Reset", ImVec2(80, 0)))
        {
            ResetSettingsToDefaults();
            m_showResetConfirmDialog = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("❌ Cancel", ImVec2(80, 0)))
        {
            m_showResetConfirmDialog = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    if (m_showResetConfirmDialog && !ImGui::IsPopupOpen("Reset Settings"))
    {
        ImGui::OpenPopup("Reset Settings");
    }
}

void MainWindow::RenderDataManagementDialogs()
{
    // Export Data Dialog
    if (m_showExportDataDialog)
    {
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Appearing);
        
        if (ImGui::BeginPopupModal("Export Data", &m_showExportDataDialog))
        {
            ImGui::Text("📤 Export your Potensio data");
            ImGui::Spacing();
            ImGui::TextWrapped("This will create a backup file containing all your settings, tasks, projects, and other data.");
            ImGui::Spacing();
            
            if (ImGui::Button("📁 Choose Location", ImVec2(150, 0)))
            {
                ExportUserData();
                m_showExportDataDialog = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine();
            if (ImGui::Button("❌ Cancel", ImVec2(80, 0)))
            {
                m_showExportDataDialog = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
        
        if (!ImGui::IsPopupOpen("Export Data"))
        {
            ImGui::OpenPopup("Export Data");
        }
    }
    
    // Import Data Dialog
    if (m_showImportDataDialog)
    {
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Appearing);
        
        if (ImGui::BeginPopupModal("Import Data", &m_showImportDataDialog))
        {
            ImGui::Text("📥 Import Potensio data");
            ImGui::Spacing();
            ImGui::TextWrapped("Select a backup file to restore your data. This will overwrite your current data.");
            ImGui::Spacing();
            
            if (ImGui::Button("📁 Select File", ImVec2(120, 0)))
            {
                ImportUserData();
                m_showImportDataDialog = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine();
            if (ImGui::Button("❌ Cancel", ImVec2(80, 0)))
            {
                m_showImportDataDialog = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
        
        if (!ImGui::IsPopupOpen("Import Data"))
        {
            ImGui::OpenPopup("Import Data");
        }
    }
    
    // Clear Data Dialog
    if (m_showClearDataDialog)
    {
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Appearing);
        
        if (ImGui::BeginPopupModal("Clear All Data", &m_showClearDataDialog))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "⚠️ DANGER: Clear all data?");
            ImGui::Spacing();
            ImGui::TextWrapped("This will permanently delete ALL your data including tasks, projects, settings, and history. This action CANNOT be undone!");
            ImGui::Spacing();
            
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 0.8f));
            if (ImGui::Button("🗑️ Delete Everything", ImVec2(150, 0)))
            {
                ClearAllUserData();
                m_showClearDataDialog = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor();
            
            ImGui::SameLine();
            if (ImGui::Button("❌ Cancel", ImVec2(80, 0)))
            {
                m_showClearDataDialog = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
        
        if (!ImGui::IsPopupOpen("Clear All Data"))
        {
            ImGui::OpenPopup("Clear All Data");
        }
    }
}

void MainWindow::RenderHotkeyEditor(const char* label, char* buffer, size_t bufferSize)
{
    ImGui::Text("%s:", label);
    ImGui::SameLine(200); // Align input fields
    
    ImGui::PushID(label);
    
    // Special styling for hotkey input
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.3f, 0.3f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.4f, 0.4f, 0.5f, 1.0f));
    
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##hotkey", buffer, bufferSize, ImGuiInputTextFlags_ReadOnly))
    {
        // Handle hotkey capture here
        // TODO: Implement hotkey capture logic
    }
    
    ImGui::PopStyleColor(3);
    
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Click and press key combination to set hotkey");
    }
    
    // Clear button
    ImGui::SameLine();
    if (ImGui::Button("❌"))
    {
        buffer[0] = '\0'; // Clear the hotkey
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Clear hotkey");
    }
    
    ImGui::PopID();
}

// Helper method implementations
void MainWindow::LoadSettingsFromConfig()
{
    if (!m_config) return;
    
    // Load general settings
    m_settingsUIState.startWithWindows = m_config->GetValue("general.start_with_windows", false);
    m_settingsUIState.startMinimized = m_config->GetValue("general.start_minimized", false);
    m_settingsUIState.minimizeToTray = m_config->GetValue("general.minimize_to_tray", true);
    m_settingsUIState.closeToTray = m_config->GetValue("general.close_to_tray", true);
    m_settingsUIState.showNotifications = m_config->GetValue("general.show_notifications", true);
    m_settingsUIState.enableSounds = m_config->GetValue("general.enable_sounds", true);
    m_settingsUIState.autoSave = m_config->GetValue("general.auto_save", true);
    m_settingsUIState.autoSaveInterval = m_config->GetValue("general.auto_save_interval", 30);
    
    // Load appearance settings
    m_settingsUIState.themeMode = m_config->GetValue("appearance.theme_mode", 0);
    m_settingsUIState.uiScale = m_config->GetValue("appearance.ui_scale", 1.0f);
    m_settingsUIState.accentColor = m_config->GetValue("appearance.accent_color", 0);
    m_settingsUIState.enableAnimations = m_config->GetValue("appearance.enable_animations", true);
    m_settingsUIState.compactMode = m_config->GetValue("appearance.compact_mode", false);
    
    // Load hotkeys
    std::string defaultPomodoro("Ctrl+Alt+P");

    std::string globalHotkey = m_config->GetValue("hotkeys.global_hotkey", defaultPomodoro);
    strcpy_s(m_settingsUIState.globalHotkeyBuffer, globalHotkey.c_str());
    
    // Load module settings
    m_settingsUIState.enablePomodoro = m_config->GetValue("modules.enable_pomodoro", true);
    m_settingsUIState.enableKanban = m_config->GetValue("modules.enable_kanban", true);
    m_settingsUIState.enableTodo = m_config->GetValue("modules.enable_todo", true);
    m_settingsUIState.enableClipboard = m_config->GetValue("modules.enable_clipboard", true);
    m_settingsUIState.enableFileConverter = m_config->GetValue("modules.enable_file_converter", true);
    m_settingsUIState.enableDropover = m_config->GetValue("modules.enable_dropover", true);
    
    Logger::Info("Settings loaded from configuration");
}

void MainWindow::SaveSettingsToConfig()
{
    if (!m_config) return;

    if (Application::GetInstance())
    {
        Application::GetInstance()->UpdateSettingsFromConfig();
    }
    
    // Save general settings
    m_config->SetValue("general.start_with_windows", m_settingsUIState.startWithWindows);
    m_config->SetValue("general.start_minimized", m_settingsUIState.startMinimized);
    m_config->SetValue("general.minimize_to_tray", m_settingsUIState.minimizeToTray);
    m_config->SetValue("general.close_to_tray", m_settingsUIState.closeToTray);
    m_config->SetValue("general.show_notifications", m_settingsUIState.showNotifications);
    m_config->SetValue("general.enable_sounds", m_settingsUIState.enableSounds);
    m_config->SetValue("general.auto_save", m_settingsUIState.autoSave);
    m_config->SetValue("general.auto_save_interval", m_settingsUIState.autoSaveInterval);
    
    // Save appearance settings
    m_config->SetValue("appearance.theme_mode", m_settingsUIState.themeMode);
    m_config->SetValue("appearance.ui_scale", m_settingsUIState.uiScale);
    m_config->SetValue("appearance.accent_color", m_settingsUIState.accentColor);
    m_config->SetValue("appearance.enable_animations", m_settingsUIState.enableAnimations);
    m_config->SetValue("appearance.compact_mode", m_settingsUIState.compactMode);
    
    // Save hotkeys
    m_config->SetValue("hotkeys.global_hotkey", std::string(m_settingsUIState.globalHotkeyBuffer));
    
    // Save module settings
    m_config->SetValue("modules.enable_pomodoro", m_settingsUIState.enablePomodoro);
    m_config->SetValue("modules.enable_kanban", m_settingsUIState.enableKanban);
    m_config->SetValue("modules.enable_todo", m_settingsUIState.enableTodo);
    m_config->SetValue("modules.enable_clipboard", m_settingsUIState.enableClipboard);
    m_config->SetValue("modules.enable_file_converter", m_settingsUIState.enableFileConverter);
    m_config->SetValue("modules.enable_dropover", m_settingsUIState.enableDropover);
    
    m_config->Save();
    Logger::Info("Settings saved to configuration");
}

void MainWindow::ResetSettingsToDefaults()
{
    m_settingsUIState = SettingsUIState(); // Reset to default state
    SaveSettingsToConfig();
    ApplyThemeSettings();
    Logger::Info("Settings reset to defaults");
}

void MainWindow::ApplyThemeSettings()
{
    // TODO: Implement theme application logic
    Logger::Info("Theme settings applied: {}", GetThemeModeName(m_settingsUIState.themeMode));
}

void MainWindow::CheckForUpdates()
{
    m_settingsUIState.checkingUpdates = true;
    // TODO: Implement actual update checking
    Logger::Info("Checking for updates...");
    
    // Simulate update check (remove in real implementation)
    // In real implementation, this would be async
    m_settingsUIState.checkingUpdates = false;
    m_settingsUIState.updateAvailable = false; // or true if update found
}

void MainWindow::ExportUserData()
{
    // TODO: Implement data export
    Logger::Info("Exporting user data...");
}

void MainWindow::ImportUserData()
{
    // TODO: Implement data import
    Logger::Info("Importing user data...");
}

void MainWindow::ClearAllUserData()
{
    // TODO: Implement data clearing
    Logger::Info("Clearing all user data...");
}

const char* MainWindow::GetThemeModeName(int mode) const
{
    switch (mode)
    {
        case 0: return "Dark";
        case 1: return "Light";
        case 2: return "Auto (System)";
        default: return "Unknown";
    }
}

const char* MainWindow::GetCategoryName(SettingsCategory category) const
{
    switch (category)
    {
        case SettingsCategory::General: return "General";
        case SettingsCategory::Appearance: return "Appearance";
        case SettingsCategory::Hotkeys: return "Hotkeys";
        case SettingsCategory::Modules: return "Modules";
        case SettingsCategory::Account: return "Account";
        case SettingsCategory::About: return "About";
        default: return "Unknown";
    }
}

ImVec4 MainWindow::GetAccentColor(int colorIndex) const
{
    const ImVec4 colors[] = {
        ImVec4(0.3f, 0.6f, 1.0f, 1.0f), // Blue
        ImVec4(0.3f, 0.8f, 0.3f, 1.0f), // Green
        ImVec4(0.7f, 0.3f, 1.0f, 1.0f), // Purple
        ImVec4(1.0f, 0.6f, 0.2f, 1.0f), // Orange
        ImVec4(1.0f, 0.3f, 0.3f, 1.0f), // Red
        ImVec4(0.2f, 0.8f, 0.8f, 1.0f), // Teal
        ImVec4(1.0f, 0.4f, 0.8f, 1.0f), // Pink
        ImVec4(1.0f, 0.9f, 0.2f, 1.0f), // Yellow
    };
    
    if (colorIndex >= 0 && colorIndex < 8)
        return colors[colorIndex];
    
    return colors[0]; // Default to blue
}

// =============================================================================
// POMODORO DATABASE IMPLEMENTATION
// =============================================================================
void MainWindow::OnPomodoroSessionStart()
{
    if (!m_pomodoroDatabase) return;
    
    auto sessionInfo = m_pomodoroTimer->GetCurrentSession();
    std::string sessionTypeStr;
    
    switch (sessionInfo.type)
    {
        case PomodoroTimer::SessionType::Work:
            sessionTypeStr = "work";
            break;
        case PomodoroTimer::SessionType::ShortBreak:
            sessionTypeStr = "short_break";
            break;
        case PomodoroTimer::SessionType::LongBreak:
            sessionTypeStr = "long_break";
            break;
    }
    
    std::string today = m_pomodoroDatabase->GetTodayDate();
    m_currentSessionId = m_pomodoroDatabase->StartSession(sessionTypeStr, sessionInfo.sessionNumber, today);
    
    Logger::Debug("Started tracking Pomodoro session {} (ID: {})", sessionTypeStr, m_currentSessionId);
}

bool MainWindow::InitializeDatabase()
{
    // Create database manager
    m_databaseManager = std::make_shared<DatabaseManager>();
    
    // Determine database path (in user's app data directory)
    std::string databasePath = GetDatabasePath();
    
    if (!m_databaseManager->Initialize(databasePath))
    {
        Logger::Error("Failed to initialize database manager");
        return false;
    }
    
    // Initialize Pomodoro database
    m_pomodoroDatabase = std::make_shared<PomodoroDatabase>(m_databaseManager);
    
    if (!m_pomodoroDatabase->Initialize())
    {
        Logger::Error("Failed to initialize Pomodoro database");
        return false;
    }
    
    Logger::Info("Database initialized successfully");
    return true;
}

std::string MainWindow::GetDatabasePath()
{
    // Get user's local app data directory
    char* appDataPath = nullptr;
    size_t len = 0;
    
    if (_dupenv_s(&appDataPath, &len, "LOCALAPPDATA") == 0 && appDataPath != nullptr)
    {
        std::string path = std::string(appDataPath) + "\\Potensio\\potensio.db";
        free(appDataPath);
        return path;
    }
    
    // Fallback to current directory
    return "potensio.db";
}

void MainWindow::LoadPomodoroConfiguration()
{
    PomodoroTimer::PomodoroConfig pomodoroConfig;
    bool loadedFromDatabase = false;
    
    // Try to load from database first
    if (m_pomodoroDatabase && m_pomodoroDatabase->ConfigurationExists())
    {
        if (m_pomodoroDatabase->LoadConfiguration(pomodoroConfig))
        {
            loadedFromDatabase = true;
            Logger::Info("Pomodoro configuration loaded from database");
        }
        else
        {
            Logger::Warning("Failed to load Pomodoro configuration from database");
        }
    }
    
    // Fallback to AppConfig if database load failed or doesn't exist
    if (!loadedFromDatabase && m_config)
    {
        pomodoroConfig.workDurationMinutes = m_config->GetValue("pomodoro.work_duration", 25);
        pomodoroConfig.shortBreakMinutes = m_config->GetValue("pomodoro.short_break", 5);
        pomodoroConfig.longBreakMinutes = m_config->GetValue("pomodoro.long_break", 15);
        pomodoroConfig.totalSessions = m_config->GetValue("pomodoro.total_sessions", 8);
        pomodoroConfig.sessionsBeforeLongBreak = m_config->GetValue("pomodoro.sessions_before_long_break", 4);
        pomodoroConfig.autoStartNextSession = m_config->GetValue("pomodoro.auto_start_next", true);
        
        // Load colors
        pomodoroConfig.colorHigh.r = m_config->GetValue("pomodoro.color_high_r", 0.0f);
        pomodoroConfig.colorHigh.g = m_config->GetValue("pomodoro.color_high_g", 1.0f);
        pomodoroConfig.colorHigh.b = m_config->GetValue("pomodoro.color_high_b", 0.0f);
        
        pomodoroConfig.colorMedium.r = m_config->GetValue("pomodoro.color_medium_r", 1.0f);
        pomodoroConfig.colorMedium.g = m_config->GetValue("pomodoro.color_medium_g", 1.0f);
        pomodoroConfig.colorMedium.b = m_config->GetValue("pomodoro.color_medium_b", 0.0f);
        
        pomodoroConfig.colorLow.r = m_config->GetValue("pomodoro.color_low_r", 1.0f);
        pomodoroConfig.colorLow.g = m_config->GetValue("pomodoro.color_low_g", 0.5f);
        pomodoroConfig.colorLow.b = m_config->GetValue("pomodoro.color_low_b", 0.0f);
        
        pomodoroConfig.colorCritical.r = m_config->GetValue("pomodoro.color_critical_r", 1.0f);
        pomodoroConfig.colorCritical.g = m_config->GetValue("pomodoro.color_critical_g", 0.0f);
        pomodoroConfig.colorCritical.b = m_config->GetValue("pomodoro.color_critical_b", 0.0f);
        
        // Save to database for future use
        if (m_pomodoroDatabase)
        {
            m_pomodoroDatabase->SaveConfiguration(pomodoroConfig);
            Logger::Info("Pomodoro configuration migrated from AppConfig to database");
        }
        
        Logger::Info("Pomodoro configuration loaded from AppConfig");
    }
    
    // Apply configuration to timer
    if (m_pomodoroTimer)
    {
        m_pomodoroTimer->SetConfig(pomodoroConfig);
    }
}

void MainWindow::SavePomodoroConfiguration(const PomodoroTimer::PomodoroConfig& config)
{
    // Save to database
    if (m_pomodoroDatabase)
    {
        if (m_pomodoroDatabase->SaveConfiguration(config))
        {
            Logger::Debug("Pomodoro configuration saved to database");
        }
        else
        {
            Logger::Warning("Failed to save Pomodoro configuration to database");
        }
    }
    
    // Also save to AppConfig as backup
    if (m_config)
    {
        m_config->SetValue("pomodoro.work_duration", config.workDurationMinutes);
        m_config->SetValue("pomodoro.short_break", config.shortBreakMinutes);
        m_config->SetValue("pomodoro.long_break", config.longBreakMinutes);
        m_config->SetValue("pomodoro.total_sessions", config.totalSessions);
        m_config->SetValue("pomodoro.sessions_before_long_break", config.sessionsBeforeLongBreak);
        m_config->SetValue("pomodoro.auto_start_next", config.autoStartNextSession);
        
        // Save colors
        m_config->SetValue("pomodoro.color_high_r", config.colorHigh.r);
        m_config->SetValue("pomodoro.color_high_g", config.colorHigh.g);
        m_config->SetValue("pomodoro.color_high_b", config.colorHigh.b);
        
        m_config->SetValue("pomodoro.color_medium_r", config.colorMedium.r);
        m_config->SetValue("pomodoro.color_medium_g", config.colorMedium.g);
        m_config->SetValue("pomodoro.color_medium_b", config.colorMedium.b);
        
        m_config->SetValue("pomodoro.color_low_r", config.colorLow.r);
        m_config->SetValue("pomodoro.color_low_g", config.colorLow.g);
        m_config->SetValue("pomodoro.color_low_b", config.colorLow.b);
        
        m_config->SetValue("pomodoro.color_critical_r", config.colorCritical.r);
        m_config->SetValue("pomodoro.color_critical_g", config.colorCritical.g);
        m_config->SetValue("pomodoro.color_critical_b", config.colorCritical.b);
        
        m_config->Save();
    }
}