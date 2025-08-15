// MainWindow.cpp - Updated with Pomodoro Integration
#include "MainWindow.h"
#include "ui/Components/Sidebar.h"
#include "ui/Windows/PomodoroWindow.h"
#include "ui/Windows/KanbanWindow.h"
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
    
    m_pomodoroSettingsWindow.reset();
    m_pomodoroTimer.reset();
    m_kanbanSettingsWindow.reset();
    m_kanbanManager.reset();
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
    
    // Project and board selector
    ImGui::Text("Project:");
    ImGui::SameLine();
    
    if (currentProject)
    {
        ImGui::Text("%s", currentProject->name.c_str());
    }
    else
    {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No project");
    }
    
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20);
    ImGui::Text("Board:");
    ImGui::SameLine();
    
    if (currentBoard)
    {
        ImGui::Text("%s", currentBoard->name.c_str());
    }
    else
    {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No board");
    }
    
    // Action buttons
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - 200);
    
    if (ImGui::Button("New Project"))
    {
        // Quick create project
        static int projectCounter = 1;
        std::string projectName = "Project " + std::to_string(projectCounter++);
        m_kanbanManager->CreateProject(projectName);
    }
    
    ImGui::SameLine();
    if (currentProject && ImGui::Button("New Board"))
    {
        // Quick create board
        static int boardCounter = 1;
        std::string boardName = "Board " + std::to_string(boardCounter++);
        m_kanbanManager->CreateBoard(boardName);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Settings"))
    {
        m_showKanbanSettings = true;
    }
    
    // Project/Board info
    if (currentProject && currentBoard)
    {
        ImGui::Spacing();
        ImGui::Text("Cards: %d | Completed: %d | Columns: %zu", 
                   currentBoard->GetTotalCardCount(),
                   currentBoard->GetCompletedCardCount(),
                   currentBoard->columns.size());
    }
}

void MainWindow::RenderKanbanBoard()
{
    auto currentBoard = m_kanbanManager->GetCurrentBoard();
    if (!currentBoard)
    {
        ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 100, 100));
        ImGui::Text("No board selected");
        ImGui::Text("Create a project and board to get started.");
        return;
    }
    
    if (currentBoard->columns.empty())
    {
        ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 100, 100));
        ImGui::Text("No columns in this board");
        ImGui::Text("Add columns in the settings to get started.");
        return;
    }
    
    // Horizontal scrolling area for columns
    float columnWidth = 280.0f;
    float columnSpacing = 16.0f;
    
    ImGui::BeginChild("##KanbanScrollArea", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    // Set cursor to start of content area
    ImGui::SetCursorPos(ImVec2(8, 8));
    
    // Render columns side by side
    for (size_t i = 0; i < currentBoard->columns.size(); ++i)
    {
        if (i > 0)
        {
            ImGui::SameLine(0, columnSpacing);
        }
        
        ImGui::BeginGroup();
        RenderKanbanColumn(currentBoard->columns[i].get(), static_cast<int>(i));
        ImGui::EndGroup();
    }
    
    ImGui::EndChild();
}

void MainWindow::RenderKanbanColumn(Kanban::Column* column, int columnIndex)
{
    if (!column) return;
    
    float columnWidth = 280.0f;
    float columnHeight = ImGui::GetContentRegionAvail().y - 50;
    
    // Column background and border
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 cursorPos = ImGui::GetCursorPos();
    
    ImVec2 columnMin = ImVec2(windowPos.x + cursorPos.x, windowPos.y + cursorPos.y);
    ImVec2 columnMax = ImVec2(columnMin.x + columnWidth, columnMin.y + columnHeight);
    
    // Column background
    ImU32 columnBgColor = IM_COL32(45, 45, 45, 255);
    ImU32 columnBorderColor = IM_COL32(80, 80, 80, 255);
    
    drawList->AddRectFilled(columnMin, columnMax, columnBgColor, 8.0f);
    drawList->AddRect(columnMin, columnMax, columnBorderColor, 8.0f, 0, 2.0f);
    
    // Column header
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12);
    
    // Column title and card count
    ImGui::Text("  %s (%zu)", column->name.c_str(), column->cards.size());
    
    // Card limit indicator
    if (column->cardLimit > 0)
    {
        ImGui::SameLine();
        if (static_cast<int>(column->cards.size()) >= column->cardLimit)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), " [FULL]");
        }
        else
        {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), " [%d/%d]", 
                             static_cast<int>(column->cards.size()), column->cardLimit);
        }
    }
    
    ImGui::PopStyleColor();
    
    // Quick add card button
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
    RenderQuickAddCard(column->id);
    
    ImGui::Spacing();
    
    // Cards area
    std::string cardsAreaId = "##CardsArea" + column->id;
    ImVec2 cardsAreaSize = ImVec2(columnWidth - 16, columnHeight - 100);
    
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);
    ImGui::BeginChild(cardsAreaId.c_str(), cardsAreaSize, false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    
    // Render cards
    for (size_t cardIndex = 0; cardIndex < column->cards.size(); ++cardIndex)
    {
        auto card = column->cards[cardIndex];
        if (card)
        {
            RenderKanbanCard(card, static_cast<int>(cardIndex), column->id);
            
            // Add spacing between cards
            if (cardIndex < column->cards.size() - 1)
            {
                ImGui::Spacing();
            }
        }
    }
    
    // Drop target at the end of the column
    RenderDropTarget(column->id, static_cast<int>(column->cards.size()));
    
    ImGui::EndChild();
}

void MainWindow::RenderKanbanCard(std::shared_ptr<Kanban::Card> card, int cardIndex, const std::string& columnId)
{
    if (!card) return;
    
    ImVec2 cardSize = ImVec2(ImGui::GetContentRegionAvail().x - 16, 0); // Height auto-calculated
    
    // Card background color based on priority
    auto priorityColor = card->GetPriorityColor();
    ImU32 cardBgColor = IM_COL32(
        static_cast<int>(priorityColor.r * 150 + 50),
        static_cast<int>(priorityColor.g * 150 + 50),
        static_cast<int>(priorityColor.b * 150 + 50),
        255
    );
    
    ImU32 cardBorderColor = IM_COL32(
        static_cast<int>(priorityColor.r * 255),
        static_cast<int>(priorityColor.g * 255),
        static_cast<int>(priorityColor.b * 255),
        255
    );
    
    // Check if this card is being dragged
    auto& dragState = m_kanbanManager->GetDragDropState();
    bool isDragging = dragState.isDragging && dragState.draggedCard && 
                     dragState.draggedCard->id == card->id;
    
    if (isDragging)
    {
        // Make dragged card slightly transparent
        cardBgColor = IM_COL32(100, 100, 100, 180);
        cardBorderColor = IM_COL32(150, 150, 150, 180);
    }
    
    // Draw card background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 cursorPos = ImGui::GetCursorPos();
    
    ImVec2 cardMin = ImVec2(windowPos.x + cursorPos.x, windowPos.y + cursorPos.y);
    
    // Begin card rendering
    ImGui::PushID(card->id.c_str());
    
    // Calculate card content height
    float cardPadding = 8.0f;
    float lineHeight = ImGui::GetTextLineHeight();
    float cardContentHeight = cardPadding * 2 + lineHeight; // Title
    
    // Add height for description if present
    if (!card->description.empty())
    {
        cardContentHeight += lineHeight + 4; // Description with spacing
    }
    
    // Add height for metadata (priority, due date)
    cardContentHeight += lineHeight + 4; // Metadata line
    
    ImVec2 cardMax = ImVec2(cardMin.x + cardSize.x, cardMin.y + cardContentHeight);
    
    // Draw card
    drawList->AddRectFilled(cardMin, cardMax, cardBgColor, 5.0f);
    drawList->AddRect(cardMin, cardMax, cardBorderColor, 5.0f, 0, 2.0f);
    
    // Card content
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cardPadding);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + cardPadding);
    
    // Card title
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::Text("%s", card->title.c_str());
    ImGui::PopStyleColor();
    
    // Card description (truncated)
    if (!card->description.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        std::string truncatedDesc = card->description;
        if (truncatedDesc.length() > 50)
        {
            truncatedDesc = truncatedDesc.substr(0, 47) + "...";
        }
        ImGui::Text("%s", truncatedDesc.c_str());
        ImGui::PopStyleColor();
    }
    
    // Priority and metadata
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(priorityColor.r, priorityColor.g, priorityColor.b, 1.0f));
    ImGui::Text("%s", GetPriorityName(static_cast<int>(card->priority)));
    
    if (!card->dueDate.empty())
    {
        ImGui::SameLine();
        ImGui::Text(" | Due: %s", card->dueDate.c_str());
    }
    ImGui::PopStyleColor();
    
    // Position cursor after card
    ImGui::SetCursorPosY(cardMin.y + cardContentHeight - windowPos.y + 4);
    
    // Handle interactions
    ImVec2 cardRegionMin = cardMin;
    ImVec2 cardRegionMax = cardMax;
    cardRegionMin.x -= windowPos.x;
    cardRegionMin.y -= windowPos.y;
    cardRegionMax.x -= windowPos.x;
    cardRegionMax.y -= windowPos.y;
    
    // Invisible button for interaction
    ImGui::SetCursorPos(ImVec2(cardRegionMin.x, cardRegionMin.y));
    bool cardClicked = ImGui::InvisibleButton("##cardbutton", ImVec2(cardRegionMax.x - cardRegionMin.x, cardRegionMax.y - cardRegionMin.y));
    
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
        ImGui::Text("Moving: %s", card->title.c_str());
        ImGui::EndDragDropSource();
    }
    
    // Context menu
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Edit"))
        {
            StartEditingCard(card);
        }
        
        ImGui::Separator();
        
        if (ImGui::MenuItem("Delete"))
        {
            m_kanbanManager->DeleteCard(card->id);
        }
        
        ImGui::EndPopup();
    }
    
    // Drop target between cards
    if (cardIndex > 0)
    {
        RenderDropTarget(columnId, cardIndex);
    }
    
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

void MainWindow::RenderQuickAddCard(const std::string& columnId)
{
    std::string buttonId = "Add Card##" + columnId;
    std::string inputId = "##quickadd" + columnId;
    
    static std::unordered_map<std::string, std::string> quickAddTexts;
    static std::unordered_map<std::string, bool> quickAddActive;
    
    bool isActive = quickAddActive[columnId];
    
    if (!isActive)
    {
        if (ImGui::Button(buttonId.c_str(), ImVec2(-1, 0)))
        {
            quickAddActive[columnId] = true;
            quickAddTexts[columnId] = "";
        }
    }
    else
    {
        // Quick add input
        char buffer[256];
        strncpy_s(buffer, quickAddTexts[columnId].c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        
        ImGui::SetKeyboardFocusHere();
        bool enterPressed = ImGui::InputText(inputId.c_str(), buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue);
        quickAddTexts[columnId] = buffer;
        
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