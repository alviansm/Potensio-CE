// Sidebar.cpp - WITH UNICODE ICONS
#include "Sidebar.h"
#include "core/Logger.h"
#include <imgui.h>

#include "resource.h"

Sidebar::Sidebar()
{
    m_width = 32.0f; // Compact width

    InitializeResources();

    // Initialize sidebar item
    m_menuItems[0] = { ModulePage::Dashboard,     "Dashboard",       "\xf0\x9f\x93\x81", true,  iconDashboard };
    m_menuItems[1] = { ModulePage::Pomodoro,      "Pomodoro",        "\xe2\x8f\xb1", true, iconTimer };
    m_menuItems[2] = { ModulePage::Kanban,        "Kanban",          "\xf0\x9f\x93\x8b", true, iconKanban };
    m_menuItems[3] = { ModulePage::Todo,          "ToDo",            "\xf0\x9f\x93\x8b", true, iconTodo };
    m_menuItems[4] = { ModulePage::Clipboard,     "Clipboard",       "\xf0\x9f\x93\x8e", true, iconClipboard };
    m_menuItems[5] = { ModulePage::FileConverter, "File Converter",  "\xf0\x9f\x94\x84", true, iconConvert };
}

Sidebar::~Sidebar()
{
    MainWindow::UnloadTexture(iconDashboard);
    MainWindow::UnloadTexture(iconTimer);
    MainWindow::UnloadTexture(iconKanban);
    MainWindow::UnloadTexture(iconTodo);
    MainWindow::UnloadTexture(iconClipboard);
    MainWindow::UnloadTexture(iconConvert);
    MainWindow::UnloadTexture(iconGear);
}

void Sidebar::Update(float deltaTime)
{
    m_animationTime += deltaTime;
}

void Sidebar::Render()
{
    // Sidebar positioning
    ImGui::SetCursorPos(ImVec2(0, 0));
    
    // Begin sidebar child window
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 8));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
    
    ImGui::BeginChild("##Sidebar", ImVec2(m_width, ImGui::GetContentRegionAvail().y), 
                     true, ImGuiWindowFlags_NoScrollbar);
    
    // Render logo/title (compact)
    RenderLogo();
    
    // Render menu items
    ImGui::Spacing();
    
    for (size_t i = 0; i < m_menuItemCount; ++i)
    {
        const SidebarItem& item = m_menuItems[i];
        m_isSelected = (item.module == m_selectedModule);
        
        RenderMenuItem(item, m_isSelected);
    }
    
    // Render footer
    RenderFooter();
    
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void Sidebar::SetSelectedModule(ModulePage module)
{
    m_selectedModule = module;
    Logger::Debug("Sidebar selection changed to: {}", static_cast<int>(module));
}

void Sidebar::InitializeResources()
{
    iconDashboard   = MainWindow::LoadTextureFromResource(IDI_DASHBOARD_ICON);
    iconTimer   = MainWindow::LoadTextureFromResource(IDI_POMODORO_ICON);
    iconKanban   = MainWindow::LoadTextureFromResource(IDI_KANBAN_ICON);
    iconTodo   = MainWindow::LoadTextureFromResource(IDI_TODO_ICON);
    iconClipboard   = MainWindow::LoadTextureFromResource(IDI_CLIPBOARD_ICON);
    iconConvert   = MainWindow::LoadTextureFromResource(IDI_CONVERTER_ICON);
    iconGear   = MainWindow::LoadTextureFromResource(IDI_GEAR_ICON);
}

void Sidebar::RenderMenuItem(const SidebarItem& item, bool isSelected)
{
    // Compact item styling
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f)); // Center align
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 8));
    
    // Colors based on state
    if (isSelected)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.45f, 0.85f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.35f, 0.75f, 1.0f));
    }
    else if (!item.enabled)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    }
    
    // Render icon button
    ImVec2 buttonSize(20, 20);
    ImGui::SetCursorPosX(2);
    
    bool clicked = false;
    if (item.enabled)
    {
        clicked = ImGui::ImageButton(item.iconTexture, buttonSize);
    }
    else
    {
        // Render disabled button
        ImGui::ImageButton(item.iconTexture, buttonSize);
    }
    
    // Show tooltip on hover
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s%s", item.name, item.enabled ? "" : " (Coming Soon)");
    }
    
    // Handle click
    if (clicked && item.enabled)
    {
        SetSelectedModule(item.module);
    }
    
    // Pop colors
    if (m_isSelected)
    {
        ImGui::PopStyleColor(3);
    }
    else if (!item.enabled)
    {
        ImGui::PopStyleColor(4);
    }
    else
    {
        ImGui::PopStyleColor(3);
    }
    
    ImGui::PopStyleVar(2);
    
    // Small spacing between items
    // ImGui::Spacing();
}

void Sidebar::RenderLogo()
{
    // Compact logo area with app icon
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 8));
    
    // App icon (using same style as sidebar items)
    ImGui::SetCursorPosX(8);
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::Text("\xf0\x9f\x92\xa0"); // 💠 App icon
    ImGui::PopFont();
    
    ImGui::PopStyleVar();
    
    // Separator
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void Sidebar::RenderFooter()
{
    // Push footer to bottom
    float footerHeight = 50.0f;
    float availableHeight = ImGui::GetContentRegionAvail().y;
    
    if (availableHeight > footerHeight)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + availableHeight - footerHeight);
    }
    
    ImGui::Separator();
    ImGui::Spacing();
    
    // Settings button (icon only)
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 6));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    
    ImGui::SetCursorPosX(4);
    if (ImGui::ImageButton(iconGear, ImVec2(20, 20))) // ⚙ Settings icon
    {
        Logger::Info("Settings clicked from sidebar");
        SetSelectedModule(ModulePage::Settings);
        ImGui::PopStyleColor(3);
    }
    else
    {
        ImGui::PopStyleColor(3);
    }
    
    // Tooltip for settings
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Settings");
    }
    
    ImGui::PopStyleVar(2);
}