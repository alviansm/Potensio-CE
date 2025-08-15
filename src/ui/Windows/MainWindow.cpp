// MainWindow.cpp - Updated with Pomodoro Integration
#include "MainWindow.h"
#include "ui/Components/Sidebar.h"
#include "ui/Windows/PomodoroWindow.h"
#include "ui/Windows/KanbanWindow.h"
#include "core/Timer/PomodoroTimer.h"
#include "core/Todo/TodoManager.h"
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
    
    m_pomodoroSettingsWindow.reset();
    m_pomodoroTimer.reset();
    m_kanbanSettingsWindow.reset();
    m_kanbanManager.reset();
    m_todoManager.reset();
    m_clipboardSettingsWindow.reset();
    m_clipboardManager.reset();
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