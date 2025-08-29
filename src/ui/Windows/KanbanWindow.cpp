#include "ui/Windows/KanbanWindow.h"
#include "app/AppConfig.h"
#include "core/Logger.h"
#include <imgui.h>
#include <algorithm>

KanbanWindow::KanbanWindow()
    : m_kanbanManager(nullptr)
    , m_config(nullptr)
    , m_isVisible(false)
    , m_showProjectManagement(false)
    , m_showBoardSettings(false)
    , m_showAppearanceSettings(false)
    , m_showImportExport(false)
    , m_showStatistics(false)
    , m_settingsChanged(false)
{
}

bool KanbanWindow::Initialize(AppConfig* config)
{
    m_config = config;
    LoadSettings();
    
    Logger::Info("KanbanWindow (Settings) initialized");
    return true;
}

void KanbanWindow::Shutdown()
{
    SaveSettings();
    Logger::Debug("KanbanWindow (Settings) shutdown");
}

void KanbanWindow::Update(float /*deltaTime*/)
{
    // Simple update for settings window
    // No complex animations needed for settings
}

void KanbanWindow::Render()
{
    if (!m_isVisible)
        return;
    
    RenderSettingsWindow();
}

void KanbanWindow::RenderSettingsWindow()
{
    ImGui::SetNextWindowSize(ImVec2(SETTINGS_WINDOW_WIDTH, SETTINGS_WINDOW_HEIGHT), ImGuiCond_FirstUseEver);
    
    bool windowOpen = m_isVisible;
    if (ImGui::Begin("Kanban Settings & Management", &windowOpen, ImGuiWindowFlags_NoCollapse))
    {
        // Tab bar for different settings categories
        if (ImGui::BeginTabBar("KanbanSettingsTabs"))
        {
            if (ImGui::BeginTabItem("Projects"))
            {
                RenderProjectManagement();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("Boards"))
            {
                RenderBoardSettings();
                ImGui::EndTabItem();
            }
            
            // if (ImGui::BeginTabItem("Appearance"))
            // {
            //     RenderAppearanceSettings();
            //     ImGui::EndTabItem();
            // }
            
            if (ImGui::BeginTabItem("Statistics"))
            {
                RenderStatistics();
                ImGui::EndTabItem();
            }
            
            // if (ImGui::BeginTabItem("Import/Export"))
            // {
            //     RenderImportExport();
            //     ImGui::EndTabItem();
            // }
            
            ImGui::EndTabBar();
        }
        
        ImGui::Separator();
        
        
        // ImGui::SameLine();
        if (ImGui::Button("Close"))
        {
            windowOpen = false;
            m_isVisible = false;
            if (m_settingsChanged)
            {
                SaveSettings();
                m_settingsChanged = false;
            }
        }
    }
    else
    {
        windowOpen = false;
    }
    ImGui::End();
    
    m_isVisible = windowOpen;
}

void KanbanWindow::RenderProjectManagement()
{
    if (!m_kanbanManager)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Kanban Manager not available");
        return;
    }
    
    // Project list header
    ImGui::Text("Projects Management");
    ImGui::Separator();
    
    if (ImGui::Button("New Project"))
    {
        m_dialogs.showNewProjectDialog = true;
        m_tempProject = TempProjectData(); // Reset
    }
    
    ImGui::SameLine();
    auto currentProject = m_kanbanManager->GetCurrentProject();
    if (currentProject && ImGui::Button("Edit Current"))
    {
        m_dialogs.showEditProjectDialog = true;
        m_tempProject.name = currentProject->name;
        m_tempProject.description = currentProject->description;
        m_tempProject.isValid = true;
    }
    
    ImGui::SameLine();
    if (currentProject && ImGui::Button("Delete Current"))
    {
        m_dialogs.showDeleteProjectConfirm = true;
        m_dialogs.targetId = currentProject->id;
        m_dialogs.targetName = currentProject->name;

        ImGui::OpenPopup("Delete Project");
    }
    
    ImGui::Spacing();
    
    // Project list
    RenderProjectList();
    
    // Project dialogs
    if (m_dialogs.showNewProjectDialog)
    {
        ImGui::SetNextWindowSize(ImVec2(DIALOG_WIDTH, DIALOG_HEIGHT), ImGuiCond_Always);
        if (ImGui::Begin("New Project", &m_dialogs.showNewProjectDialog, ImGuiWindowFlags_Modal))
        {
            static char nameBuffer[256] = "";
            static char descBuffer[512] = "";
            
            ImGui::Text("Create New Project");
            ImGui::Separator();
            
            ImGui::Text("Name:");
            ImGui::InputText("##ProjectName", nameBuffer, sizeof(nameBuffer));
            
            ImGui::Text("Description:");
            ImGui::InputTextMultiline("##ProjectDesc", descBuffer, sizeof(descBuffer), ImVec2(-1, 100));
            
            ImGui::Spacing();
            
            bool canCreate = strlen(nameBuffer) > 0;
            if (!canCreate) ImGui::BeginDisabled();
            
            if (ImGui::Button("Create"))
            {
                m_kanbanManager->CreateProject(nameBuffer, descBuffer);
                m_dialogs.showNewProjectDialog = false;
                nameBuffer[0] = '\0';
                descBuffer[0] = '\0';
            }
            
            if (!canCreate) ImGui::EndDisabled();
            
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                m_dialogs.showNewProjectDialog = false;
                nameBuffer[0] = '\0';
                descBuffer[0] = '\0';
            }
        }
        ImGui::End();
    }

    // Edit project dialog
    if (m_dialogs.showEditProjectDialog)
    {
        ImGui::SetNextWindowSize(ImVec2(DIALOG_WIDTH, DIALOG_HEIGHT), ImGuiCond_Always);
        if (ImGui::Begin("Edit Project", &m_dialogs.showEditProjectDialog, ImGuiWindowFlags_Modal))
        {
            static char nameBuffer[256] = "";
            static char descBuffer[512] = "";
            
            // Initialize buffers on first open
            if (nameBuffer[0] == '\0' && m_tempProject.isValid)
            {
                strncpy(nameBuffer, m_tempProject.name.c_str(), sizeof(nameBuffer));
                strncpy(descBuffer, m_tempProject.description.c_str(), sizeof(descBuffer));
            }
            
            ImGui::Text("Edit Project");
            ImGui::Separator();
            
            ImGui::Text("Name:");
            ImGui::InputText("##ProjectName", nameBuffer, sizeof(nameBuffer));
            
            ImGui::Text("Description:");
            ImGui::InputTextMultiline("##ProjectDesc", descBuffer, sizeof(descBuffer), ImVec2(-1, 100));
            
            ImGui::Spacing();
            
            bool canSave = strlen(nameBuffer) > 0;
            if (!canSave) ImGui::BeginDisabled();
            
            if (ImGui::Button("Save"))
            {
                if (currentProject)
                {
                    currentProject->name = nameBuffer;
                    currentProject->description = descBuffer;
                    m_kanbanManager->UpdateProject(*currentProject);
                }
                m_dialogs.showEditProjectDialog = false;
                nameBuffer[0] = '\0';
                descBuffer[0] = '\0';
                m_tempProject = TempProjectData(); // Reset
            }
            
            if (!canSave) ImGui::EndDisabled();
            
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                m_dialogs.showEditProjectDialog = false;
                nameBuffer[0] = '\0';
                descBuffer[0] = '\0';
                m_tempProject = TempProjectData(); // Reset
            }
        }
        ImGui::End();
    }
    
    // Delete confirmation (Manual creation)
    if (ImGui::BeginPopupModal("Delete Project", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Are you sure you want to delete the project '%s'?\nThis action cannot be undone.", m_dialogs.targetName.c_str());
        ImGui::Separator();
        
        if (ImGui::Button("Delete"))
        {
            if (!m_dialogs.targetId.empty())
            {
                m_kanbanManager->DeleteProject(m_dialogs.targetId);
            }
            m_dialogs.showDeleteProjectConfirm = false;
            m_dialogs.targetId.clear();
            m_dialogs.targetName.clear();
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            m_dialogs.showDeleteProjectConfirm = false;
            m_dialogs.targetId.clear();
            m_dialogs.targetName.clear();
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void KanbanWindow::RenderBoardSettings()
{
    if (!m_kanbanManager)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Kanban Manager not available");
        return;
    }
    
    auto currentProject = m_kanbanManager->GetCurrentProject();
    if (!currentProject)
    {
        ImGui::Text("No project selected");
        return;
    }
    
    ImGui::Text("Board Management for: %s", currentProject->name.c_str());
    ImGui::Separator();
    
    if (ImGui::Button("New Board"))
    {
        m_dialogs.showNewBoardDialog = true;
        m_tempBoard = TempBoardData(); // Resets
    }

    // New Board dialog
    if (m_dialogs.showNewBoardDialog)
    {
        ImGui::SetNextWindowSize(ImVec2(DIALOG_WIDTH, DIALOG_HEIGHT), ImGuiCond_Always);
        if (ImGui::Begin("New Board", &m_dialogs.showNewBoardDialog, ImGuiWindowFlags_Modal))
        {
            static char nameBuffer[256] = "";
            static char descBuffer[512] = "";
            
            ImGui::Text("Create New Board");
            ImGui::Separator();
            
            ImGui::Text("Name:");
            ImGui::InputText("##BoardName", nameBuffer, sizeof(nameBuffer));
            
            ImGui::Text("Description:");
            ImGui::InputTextMultiline("##BoardDesc", descBuffer, sizeof(descBuffer), ImVec2(-1, 100));
            
            ImGui::Spacing();
            
            bool canCreate = strlen(nameBuffer) > 0;
            if (!canCreate) ImGui::BeginDisabled();
            
            if (ImGui::Button("Create"))
            {
                m_kanbanManager->CreateBoard(nameBuffer, descBuffer);
                m_dialogs.showNewBoardDialog = false;
                nameBuffer[0] = '\0';
                descBuffer[0] = '\0';
            }
            
            if (!canCreate) ImGui::EndDisabled();
            
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                m_dialogs.showNewBoardDialog = false;
                nameBuffer[0] = '\0';
                descBuffer[0] = '\0';
            }
        }
        ImGui::End();
    }
    
    ImGui::SameLine();
    auto currentBoard = m_kanbanManager->GetCurrentBoard();
    if (currentBoard && ImGui::Button("Edit Current"))
    {
        m_dialogs.showEditBoardDialog = true;
        m_tempBoard.name = currentBoard->name;
        m_tempBoard.description = currentBoard->description;
        m_tempBoard.isValid = true;
    }

    // Edit Current board dialog
    if (m_dialogs.showEditBoardDialog)
    {
        ImGui::SetNextWindowSize(ImVec2(DIALOG_WIDTH, DIALOG_HEIGHT), ImGuiCond_Always);
        if (ImGui::Begin("Edit Board", &m_dialogs.showEditBoardDialog, ImGuiWindowFlags_Modal))
        {
            static char nameBuffer[256] = "";
            static char descBuffer[512] = "";
            
            // Initialize buffers on first open
            if (nameBuffer[0] == '\0' && m_tempBoard.isValid)
            {
                strncpy(nameBuffer, m_tempBoard.name.c_str(), sizeof(nameBuffer));
                strncpy(descBuffer, m_tempBoard.description.c_str(), sizeof(descBuffer));
            }
            
            ImGui::Text("Edit Board");
            ImGui::Separator();
            
            ImGui::Text("Name:");
            ImGui::InputText("##BoardName", nameBuffer, sizeof(nameBuffer));
            
            ImGui::Text("Description:");
            ImGui::InputTextMultiline("##BoardDesc", descBuffer, sizeof(descBuffer), ImVec2(-1, 100));
            
            ImGui::Spacing();
            
            bool canSave = strlen(nameBuffer) > 0;
            if (!canSave) ImGui::BeginDisabled();
            
            if (ImGui::Button("Save"))
            {
                if (currentBoard)
                {
                    currentBoard->name = nameBuffer;
                    currentBoard->description = descBuffer;
                    m_kanbanManager->UpdateBoard(*currentBoard);
                }
                m_dialogs.showEditBoardDialog = false;
                nameBuffer[0] = '\0';
                descBuffer[0] = '\0';
                m_tempBoard = TempBoardData(); // Reset
            }
            
            if (!canSave) ImGui::EndDisabled();
            
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                m_dialogs.showEditBoardDialog = false;
                nameBuffer[0] = '\0';
                descBuffer[0] = '\0';
                m_tempBoard = TempBoardData(); // Reset
            }
        }
        ImGui::End();
    }
    
    ImGui::SameLine();
    if (currentBoard && ImGui::Button("Delete Current"))
    {
        m_dialogs.showDeleteBoardConfirm = true;
        m_dialogs.targetId = currentBoard->id;
        m_dialogs.targetName = currentBoard->name;

        ImGui::OpenPopup("Delete Board"); // <-- this is crucial
    }

    // Show delete confirmation dialog prior deleting the board
    if (ImGui::BeginPopupModal("Delete Board", NULL, ImGuiWindowFlags_NoResize))
    {
        ImGui::Text("Are you sure you want to delete board '%s'?", m_dialogs.targetName.c_str());
        ImGui::Separator();
        ImGui::Spacing();
        
        float buttonWidth = 100.0f;
        if (ImGui::Button("Yes", ImVec2(buttonWidth, 0)))
        {
            m_kanbanManager->DeleteBoard(m_dialogs.targetId);
            ImGui::CloseCurrentPopup();
            m_dialogs.showDeleteBoardConfirm = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(buttonWidth, 0)))
        {
            ImGui::CloseCurrentPopup();
            m_dialogs.showDeleteBoardConfirm = false;
        }

        ImGui::EndPopup();
    }
    
    ImGui::Spacing();
    
    // Board list
    RenderBoardList();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Column editor
    if (currentBoard)
    {
        RenderColumnEditor();
    }
}

void KanbanWindow::RenderAppearanceSettings()
{
    ImGui::Text("Appearance & Visual Settings");
    ImGui::Separator();
    
    if (ImGui::CollapsingHeader("Colors", ImGuiTreeNodeFlags_DefaultOpen))
    {
        RenderColorPicker("Default Card Color", m_appearanceSettings.defaultCardColor);
        RenderColorPicker("Default Column Color", m_appearanceSettings.defaultColumnColor);
    }
    
    if (ImGui::CollapsingHeader("Layout", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool changed = false;
        
        changed |= ImGui::SliderFloat("Card Rounding", &m_appearanceSettings.cardRounding, 0.0f, 15.0f, "%.1f");
        changed |= ImGui::SliderFloat("Column Rounding", &m_appearanceSettings.columnRounding, 0.0f, 20.0f, "%.1f");
        changed |= ImGui::SliderFloat("Card Spacing", &m_appearanceSettings.cardSpacing, 2.0f, 20.0f, "%.1f");
        changed |= ImGui::SliderFloat("Column Spacing", &m_appearanceSettings.columnSpacing, 8.0f, 40.0f, "%.1f");
        
        if (changed) m_settingsChanged = true;
    }
    
    if (ImGui::CollapsingHeader("Display Options", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool changed = false;
        
        changed |= ImGui::Checkbox("Show Card Numbers", &m_appearanceSettings.showCardNumbers);
        changed |= ImGui::Checkbox("Show Due Dates", &m_appearanceSettings.showDueDates);
        changed |= ImGui::Checkbox("Enable Drag/Drop Animation", &m_appearanceSettings.enableDragDropAnimation);
        changed |= ImGui::Checkbox("Compact Mode", &m_appearanceSettings.compactMode);
        
        if (changed) m_settingsChanged = true;
    }
    
    ImGui::Spacing();
    
    if (ImGui::Button("Preview Changes"))
    {
        Logger::Info("Preview appearance changes");
        // TODO: Apply temporary visual changes
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Reset Appearance"))
    {
        m_appearanceSettings = AppearanceSettings(); // Reset to defaults
        m_settingsChanged = true;
    }
}

void KanbanWindow::RenderImportExport()
{
    ImGui::Text("Import & Export");
    ImGui::Separator();
    
    ImGui::Text("Project Operations");
    
    if (ImGui::Button("Save Current Project..."))
    {
        OpenSaveProjectDialog();
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Save current project to XML file");
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Load Project..."))
    {
        OpenLoadProjectDialog();
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Load project from XML file");
    }
    
    ImGui::Spacing();
    
    ImGui::Text("Board Operations");
    
    auto currentBoard = m_kanbanManager ? m_kanbanManager->GetCurrentBoard() : nullptr;
    if (!currentBoard) ImGui::BeginDisabled();
    
    if (ImGui::Button("Export Current Board..."))
    {
        OpenExportBoardDialog();
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Export current board to various formats");
    }
    
    if (!currentBoard) ImGui::EndDisabled();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Text("Backup & Restore");
    
    if (ImGui::Button("Create Backup..."))
    {
        Logger::Info("Create backup requested");
        // TODO: Implement backup functionality
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Restore from Backup..."))
    {
        Logger::Info("Restore backup requested");
        // TODO: Implement restore functionality
    }
    
    ImGui::Spacing();
    
    ImGui::TextWrapped("Note: Projects are saved in XML format. Boards can be exported as CSV, JSON, or PDF reports.");
}

void KanbanWindow::RenderStatistics()
{
    if (!m_kanbanManager)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Kanban Manager not available");
        return;
    }
    
    auto stats = m_kanbanManager->GetStatistics();
    
    ImGui::Text("Kanban Statistics");
    ImGui::Separator();
    
    // Overview
    ImGui::Text("Total Projects: %d", stats.totalProjects);
    ImGui::Text("Total Boards: %d", stats.totalBoards);
    ImGui::Text("Total Cards: %d", stats.totalCards);
    ImGui::Text("Completed Cards: %d", stats.completedCards);
    ImGui::Text("Completion Rate: %.1f%%", stats.completionRate);
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Current project stats
    auto currentProject = m_kanbanManager->GetCurrentProject();
    if (currentProject)
    {
        ImGui::Text("Current Project: %s", currentProject->name.c_str());
        ImGui::Text("Boards in Project: %d", currentProject->GetTotalBoardCount());
        ImGui::Text("Cards in Project: %d", currentProject->GetTotalCardCount());
        ImGui::Text("Completed in Project: %d", currentProject->GetCompletedCardCount());
        
        auto currentBoard = m_kanbanManager->GetCurrentBoard();
        if (currentBoard)
        {
            ImGui::Spacing();
            ImGui::Text("Current Board: %s", currentBoard->name.c_str());
            ImGui::Text("Columns: %zu", currentBoard->columns.size());
            ImGui::Text("Cards in Board: %d", currentBoard->GetTotalCardCount());
            ImGui::Text("Completed in Board: %d", currentBoard->GetCompletedCardCount());
            ImGui::Text("Overdue Cards: %d", currentBoard->GetOverdueCardCount());
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    if (ImGui::Button("Export Statistics"))
    {
        Logger::Info("Export statistics requested");
        // TODO: Export statistics to file
    }
    
    // ImGui::SameLine();
    // if (ImGui::Button("Clear All Statistics"))
    // {
    //     Logger::Info("Clear statistics requested");
    //     // TODO: Clear statistics
    // }
}

void KanbanWindow::RenderProjectList()
{
    if (!m_kanbanManager) return;
    
    const auto& projects = m_kanbanManager->GetProjects();
    auto currentProject = m_kanbanManager->GetCurrentProject();
    
    if (ImGui::BeginTable("ProjectTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Boards", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Cards", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableHeadersRow();
        
        for (const auto& project : projects)
        {
            if (!project) continue;
            
            ImGui::TableNextRow();
            
            bool isCurrent = currentProject && currentProject->id == project->id;
            if (isCurrent)
            {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(100, 150, 100, 100));
            }
            
            ImGui::TableNextColumn();
            if (ImGui::Selectable(project->name.c_str(), isCurrent, ImGuiSelectableFlags_SpanAllColumns))
            {
                m_kanbanManager->SetCurrentProject(project->id);
            }
            
            ImGui::TableNextColumn();
            ImGui::Text("%d", project->GetTotalBoardCount());
            
            ImGui::TableNextColumn();
            ImGui::Text("%d", project->GetTotalCardCount());
        }
        
        ImGui::EndTable();
    }
}

void KanbanWindow::RenderBoardList()
{
    auto currentProject = m_kanbanManager->GetCurrentProject();
    if (!currentProject) return;
    
    auto currentBoard = m_kanbanManager->GetCurrentBoard();
    
    if (ImGui::BeginTable("BoardTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Columns", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Cards", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();
        
        for (const auto& board : currentProject->boards)
        {
            if (!board) continue;
            
            ImGui::TableNextRow();
            
            bool isCurrent = currentBoard && currentBoard->id == board->id;
            if (isCurrent)
            {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(100, 150, 100, 100));
            }
            
            ImGui::TableNextColumn();
            if (ImGui::Selectable(board->name.c_str(), isCurrent, ImGuiSelectableFlags_SpanAllColumns))
            {
                m_kanbanManager->SetCurrentBoard(board->id);
            }
            
            ImGui::TableNextColumn();
            ImGui::Text("%zu", board->columns.size());
            
            ImGui::TableNextColumn();
            ImGui::Text("%d", board->GetTotalCardCount());
            
            ImGui::TableNextColumn();
            ImGui::Text("%s", board->isActive ? "Active" : "Inactive");
        }
        
        ImGui::EndTable();
    }
}

void KanbanWindow::RenderColumnEditor()
{
    auto currentBoard = m_kanbanManager->GetCurrentBoard();
    if (!currentBoard) return;
    
    ImGui::Text("Column Management");
    ImGui::Separator();
    
    // if (ImGui::Button("Add Column"))
    // {
    //     m_dialogs.showNewColumnDialog = true;
    //     m_tempColumn = TempColumnData(); // Reset
    // }
    
    // Column list
    if (ImGui::BeginTable("ColumnTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Cards", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Limit", ImGuiTableColumnFlags_WidthFixed, 60);
        // ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableHeadersRow();
        
        for (size_t i = 0; i < currentBoard->columns.size(); ++i)
        {
            const auto& column = currentBoard->columns[i];
            if (!column) continue;
            
            ImGui::TableNextRow();
            
            ImGui::TableNextColumn();
            ImGui::Text("%s", column->name.c_str());
            
            ImGui::TableNextColumn();
            ImGui::Text("%zu", column->cards.size());
            
            ImGui::TableNextColumn();
            ImGui::Text("%s", column->cardLimit < 0 ? "∞" : std::to_string(column->cardLimit).c_str());
            
            // ImGui::TableNextColumn();
            // std::string editId = "Edit##" + std::to_string(i);
            // std::string deleteId = "Delete##" + std::to_string(i);
            
            // if (ImGui::SmallButton(editId.c_str()))
            // {
            //     // TODO: Edit column
            // }
            
            // ImGui::SameLine();
            // if (ImGui::SmallButton(deleteId.c_str()))
            // {
            //     // TODO: Delete column with confirmation
            // }
        }
        
        ImGui::EndTable();
    }
}

void KanbanWindow::RenderCardTemplates()
{
    // TODO: Implement card templates
    ImGui::Text("Card Templates (Coming Soon)");
    ImGui::TextWrapped("Create reusable card templates with predefined titles, descriptions, and priorities.");
}

void KanbanWindow::RenderColorPicker(const char* label, Kanban::Color& color)
{
    float colorArray[4] = { color.r, color.g, color.b, color.a };
    
    if (ImGui::ColorEdit4(label, colorArray))
    {
        color.r = colorArray[0];
        color.g = colorArray[1];
        color.b = colorArray[2];
        color.a = colorArray[3];
        m_settingsChanged = true;
    }
}

void KanbanWindow::RenderPrioritySelector(const char* label, Kanban::Priority& priority)
{
    const char* priorityNames[] = { "Low", "Medium", "High", "Urgent" };
    int currentPriority = static_cast<int>(priority);
    
    if (ImGui::Combo(label, &currentPriority, priorityNames, IM_ARRAYSIZE(priorityNames)))
    {
        priority = static_cast<Kanban::Priority>(currentPriority);
        m_settingsChanged = true;
    }
}

bool KanbanWindow::RenderConfirmationDialog(const char* title, const char* message, bool* open)
{
    bool result = false;

    if (*open)    
        ImGui::OpenPopup(title);    

    if (ImGui::BeginPopupModal(title, open, ImGuiWindowFlags_NoResize))
    {
        ImGui::SetWindowSize(ImVec2(300, 150), ImGuiCond_Always);
        ImGui::SetWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always);

        ImGui::TextWrapped("%s", message);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float buttonWidth = 80.0f;
        float windowWidth = ImGui::GetWindowWidth();
        ImGui::SetCursorPosX((windowWidth - buttonWidth * 2 - ImGui::GetStyle().ItemSpacing.x) * 0.5f);

        if (ImGui::Button("Yes", ImVec2(buttonWidth, 0)))
        {
            result = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(buttonWidth, 0)))
        {
            result = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return result;
}

void KanbanWindow::LoadSettings()
{
    if (!m_config) return;
    
    // Load appearance settings
    m_appearanceSettings.defaultCardColor.r = m_config->GetValue("kanban.appearance.card_color_r", 0.9f);
    m_appearanceSettings.defaultCardColor.g = m_config->GetValue("kanban.appearance.card_color_g", 0.9f);
    m_appearanceSettings.defaultCardColor.b = m_config->GetValue("kanban.appearance.card_color_b", 0.9f);
    m_appearanceSettings.defaultCardColor.a = m_config->GetValue("kanban.appearance.card_color_a", 1.0f);
    
    m_appearanceSettings.cardRounding = m_config->GetValue("kanban.appearance.card_rounding", 5.0f);
    m_appearanceSettings.columnRounding = m_config->GetValue("kanban.appearance.column_rounding", 8.0f);
    m_appearanceSettings.cardSpacing = m_config->GetValue("kanban.appearance.card_spacing", 8.0f);
    m_appearanceSettings.columnSpacing = m_config->GetValue("kanban.appearance.column_spacing", 16.0f);
    
    m_appearanceSettings.showCardNumbers = m_config->GetValue("kanban.appearance.show_card_numbers", true);
    m_appearanceSettings.showDueDates = m_config->GetValue("kanban.appearance.show_due_dates", true);
    m_appearanceSettings.enableDragDropAnimation = m_config->GetValue("kanban.appearance.enable_animations", true);
    m_appearanceSettings.compactMode = m_config->GetValue("kanban.appearance.compact_mode", false);
    
    Logger::Debug("Kanban window settings loaded");
}

void KanbanWindow::SaveSettings()
{
    if (!m_config) return;
    
    // Save appearance settings
    m_config->SetValue("kanban.appearance.card_color_r", m_appearanceSettings.defaultCardColor.r);
    m_config->SetValue("kanban.appearance.card_color_g", m_appearanceSettings.defaultCardColor.g);
    m_config->SetValue("kanban.appearance.card_color_b", m_appearanceSettings.defaultCardColor.b);
    m_config->SetValue("kanban.appearance.card_color_a", m_appearanceSettings.defaultCardColor.a);
    
    m_config->SetValue("kanban.appearance.card_rounding", m_appearanceSettings.cardRounding);
    m_config->SetValue("kanban.appearance.column_rounding", m_appearanceSettings.columnRounding);
    m_config->SetValue("kanban.appearance.card_spacing", m_appearanceSettings.cardSpacing);
    m_config->SetValue("kanban.appearance.column_spacing", m_appearanceSettings.columnSpacing);
    
    m_config->SetValue("kanban.appearance.show_card_numbers", m_appearanceSettings.showCardNumbers);
    m_config->SetValue("kanban.appearance.show_due_dates", m_appearanceSettings.showDueDates);
    m_config->SetValue("kanban.appearance.enable_animations", m_appearanceSettings.enableDragDropAnimation);
    m_config->SetValue("kanban.appearance.compact_mode", m_appearanceSettings.compactMode);
    
    m_config->Save();
    Logger::Debug("Kanban window settings saved");
}

void KanbanWindow::ResetToDefaults()
{
    m_appearanceSettings = AppearanceSettings();
    m_settingsChanged = true;
    Logger::Info("Kanban settings reset to defaults");
}

void KanbanWindow::OpenSaveProjectDialog()
{
    Logger::Info("Save project dialog requested");
    // TODO: Implement file dialog
}

void KanbanWindow::OpenLoadProjectDialog()
{
    Logger::Info("Load project dialog requested");
    // TODO: Implement file dialog
}

void KanbanWindow::OpenExportBoardDialog()
{
    Logger::Info("Export board dialog requested");
    // TODO: Implement export dialog
}