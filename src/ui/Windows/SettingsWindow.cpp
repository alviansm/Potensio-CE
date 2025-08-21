#include "SettingsWindow.h"
#include "app/Application.h"
#include "app/AppConfig.h"
#include "core/Logger.h"
#include <imgui.h>

SettingsWindow::SettingsWindow()
{
    LoadCurrentSettings();
}

SettingsWindow::~SettingsWindow()
{
}

void SettingsWindow::Update(float /*deltaTime*/)  // Suppress unused parameter warning
{
    // Settings window doesn't need continuous updates
}

void SettingsWindow::Render()
{
    if (!m_isVisible)
        return;
        
    // Center the settings window
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_Appearing);
    
    // Window flags (removed ImGuiWindowFlags_NoDocking for compatibility)
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
    
    if (ImGui::Begin("Settings", &m_isVisible, flags))
    {
        // Check if we need to save settings when closing
        if (!m_isVisible && HasUnsavedChanges())
        {
            ImGui::OpenPopup("UnsavedChanges");
            m_isVisible = true; // Keep window open until user decides
        }
        
        // Main settings layout
        ImGui::Columns(2, "SettingsColumns", true);
        ImGui::SetColumnWidth(0, 150);
        
        // Category selector (left column)
        RenderCategorySelector();
        
        ImGui::NextColumn();
        
        // Settings content (right column)
        ImGui::BeginChild("SettingsContent", ImVec2(0, -40), false);
        
        switch (m_selectedCategory)
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
            case SettingsCategory::About:
                RenderAboutSection();
                break;
        }
        
        ImGui::EndChild();
        
        // Buttons (bottom of right column)
        RenderButtons();
        
        ImGui::Columns(1);
        
        // Unsaved changes popup
        if (ImGui::BeginPopupModal("UnsavedChanges", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("You have unsaved changes. What would you like to do?");
            ImGui::Spacing();
            
            if (ImGui::Button("Save & Close", ImVec2(120, 0)))
            {
                ApplySettings();
                ImGui::CloseCurrentPopup();
                m_isVisible = false;
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Discard Changes", ImVec2(120, 0)))
            {
                LoadCurrentSettings();
                ImGui::CloseCurrentPopup();
                m_isVisible = false;
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
                // m_isVisible stays true
            }
            
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

void SettingsWindow::RenderCategorySelector()
{
    ImGui::Text("Categories");
    ImGui::Separator();
    ImGui::Spacing();
    
    const SettingsCategory categories[] = {
        SettingsCategory::General,
        SettingsCategory::Appearance,
        SettingsCategory::Hotkeys,
        SettingsCategory::About
    };
    
    for (const auto& category : categories)
    {
        bool isSelected = (category == m_selectedCategory);
        
        if (isSelected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
        }
        
        if (ImGui::Button(GetCategoryName(category), ImVec2(-1, 30)))
        {
            m_selectedCategory = category;
        }
        
        if (isSelected)
        {
            ImGui::PopStyleColor();
        }
    }
}

void SettingsWindow::RenderGeneralSettings()
{
    ImGui::Text("General Settings");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Startup options
    ImGui::Text("Startup");
    bool startWithWindows = m_tempSettings.startWithWindows;
    if (ImGui::Checkbox("Start with Windows", &startWithWindows))
    {
        m_tempSettings.startWithWindows = startWithWindows;
        m_settingsChanged = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Automatically start Potensio when Windows starts");
    }
    
    ImGui::Spacing();
    
    // Window behavior
    ImGui::Text("Window Behavior");
    bool minimizeToTray = m_tempSettings.minimizeToTray;
    if (ImGui::Checkbox("Minimize to system tray", &minimizeToTray))
    {
        m_tempSettings.minimizeToTray = minimizeToTray;
        m_settingsChanged = true;
    }
    
    bool showNotifications = m_tempSettings.showNotifications;
    if (ImGui::Checkbox("Show notifications", &showNotifications))
    {
        m_tempSettings.showNotifications = showNotifications;
        m_settingsChanged = true;
    }
    
    ImGui::Spacing();
    ImGui::Spacing();
    
    // Application info
    ImGui::Text("Application Information");
    ImGui::BulletText("Version: 0.1.0 (Sprint 1)");
    ImGui::BulletText("Build: Debug");
    ImGui::BulletText("Framework: Dear ImGui + OpenGL");
}

void SettingsWindow::RenderAppearanceSettings()
{
    ImGui::Text("Appearance Settings");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Font size
    ImGui::Text("Font & UI");
    float fontSize = m_tempSettings.fontSize;
    if (ImGui::SliderFloat("Font Size", &fontSize, 10.0f, 20.0f, "%.1f px"))
    {
        m_tempSettings.fontSize = fontSize;
        m_settingsChanged = true;
    }
    
    // Sidebar width
    int sidebarWidth = m_tempSettings.sidebarWidth;
    if (ImGui::SliderInt("Sidebar Width", &sidebarWidth, 150, 250, "%d px"))
    {
        m_tempSettings.sidebarWidth = sidebarWidth;
        m_settingsChanged = true;
    }
    
    ImGui::Spacing();
    
    // Theme selection
    ImGui::Text("Theme");
    const char* themes[] = { "Dark", "Light", "Custom" };
    int currentTheme = 0; // Default to dark
    if (m_tempSettings.theme == "light") currentTheme = 1;
    else if (m_tempSettings.theme == "custom") currentTheme = 2;
    
    if (ImGui::Combo("Theme", &currentTheme, themes, 3))
    {
        switch (currentTheme)
        {
            case 0: m_tempSettings.theme = "dark"; break;
            case 1: m_tempSettings.theme = "light"; break;
            case 2: m_tempSettings.theme = "custom"; break;
        }
        m_settingsChanged = true;
    }
    
    ImGui::Spacing();
    ImGui::TextWrapped("Note: Some appearance changes require a restart to take full effect.");
}

void SettingsWindow::RenderHotkeySettings()
{
    ImGui::Text("Hotkey Settings");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Global hotkey enable/disable
    bool hotkeyEnabled = m_tempSettings.hotkeyEnabled;
    if (ImGui::Checkbox("Enable global hotkey", &hotkeyEnabled))
    {
        m_tempSettings.hotkeyEnabled = hotkeyEnabled;
        m_settingsChanged = true;
    }
    
    if (hotkeyEnabled)
    {
        ImGui::Spacing();
        ImGui::Text("Toggle Window Hotkey");
        
        // Modifier keys
        bool ctrlPressed = (m_tempSettings.hotkeyModifiers & MOD_CONTROL) != 0;
        bool shiftPressed = (m_tempSettings.hotkeyModifiers & MOD_SHIFT) != 0;
        bool altPressed = (m_tempSettings.hotkeyModifiers & MOD_ALT) != 0;
        
        if (ImGui::Checkbox("Ctrl", &ctrlPressed))
        {
            if (ctrlPressed)
                m_tempSettings.hotkeyModifiers |= MOD_CONTROL;
            else
                m_tempSettings.hotkeyModifiers &= ~MOD_CONTROL;
            m_settingsChanged = true;
        }
        
        ImGui::SameLine();
        if (ImGui::Checkbox("Shift", &shiftPressed))
        {
            if (shiftPressed)
                m_tempSettings.hotkeyModifiers |= MOD_SHIFT;
            else
                m_tempSettings.hotkeyModifiers &= ~MOD_SHIFT;
            m_settingsChanged = true;
        }
        
        ImGui::SameLine();
        if (ImGui::Checkbox("Alt", &altPressed))
        {
            if (altPressed)
                m_tempSettings.hotkeyModifiers |= MOD_ALT;
            else
                m_tempSettings.hotkeyModifiers &= ~MOD_ALT;
            m_settingsChanged = true;
        }
        
        // Key selection
        ImGui::Text("Key: %c", static_cast<char>(m_tempSettings.hotkeyKey));
        ImGui::SameLine();
        if (ImGui::Button("Change Key"))
        {
            ImGui::OpenPopup("ChangeKey");
        }
        
        // Key change popup
        if (ImGui::BeginPopupModal("ChangeKey", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Press a key (A-Z, 0-9):");
            ImGui::Spacing();
            
            // Simple key detection (in a real implementation, you'd use proper key capture)
            for (int key = 'A'; key <= 'Z'; ++key)
            {
                char keyChar = static_cast<char>(key);
                if (ImGui::Button(&keyChar, ImVec2(30, 30)))
                {
                    m_tempSettings.hotkeyKey = key;
                    m_settingsChanged = true;
                    ImGui::CloseCurrentPopup();
                }
                if ((key - 'A') % 6 != 5) ImGui::SameLine();
            }
            
            ImGui::Spacing();
            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
        
        // Current hotkey display
        ImGui::Spacing();
        ImGui::Text("Current hotkey: %s%s%s%c",
                   ctrlPressed ? "Ctrl+" : "",
                   shiftPressed ? "Shift+" : "",
                   altPressed ? "Alt+" : "",
                   static_cast<char>(m_tempSettings.hotkeyKey));
    }
    else
    {
        ImGui::TextWrapped("Global hotkey is disabled. Enable it to set up a keyboard shortcut for showing/hiding Potensio.");
    }
}

void SettingsWindow::RenderAboutSection()
{
    ImGui::Text("About Potensio");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Logo/Title
    ImGui::Text("Potensio - Productivity Suite");
    ImGui::Text("All-in-One File Management & Productivity");
    ImGui::Spacing();
    
    // Version info
    ImGui::Text("Version: 0.1.0 - Experimental");
    ImGui::Text("Build Date: %s", __DATE__);
    ImGui::Spacing();
    
    // Features
    ImGui::Text("Current Features:");
    ImGui::BulletText("Dropover File Management");
    ImGui::BulletText("Compact Icon-Based Navigation");
    ImGui::BulletText("System Tray Integration");
    ImGui::BulletText("Global Hotkey Support");
    ImGui::Spacing();
    
    ImGui::Text("Planned Features:");
    ImGui::BulletText("Pomodoro Timer");
    ImGui::BulletText("Kanban Board");
    ImGui::BulletText("Clipboard Manager");
    ImGui::BulletText("Bulk File Renaming");
    ImGui::BulletText("File Conversion & Compression");
    ImGui::Spacing();
    
    // Technical info
    ImGui::Text("Built with:");
    ImGui::BulletText("C++17");
    ImGui::BulletText("Dear ImGui");
    ImGui::BulletText("OpenGL");
    ImGui::BulletText("SQLite (coming in Sprint 2)");
    ImGui::Spacing();
    
    // Sprint progress
    ImGui::Text("Development Progress:");
    ImGui::BulletText("DONE: Sprint 1 - Foundation & UI Framework");
    ImGui::BulletText("NEXT: Sprint 2 - Database & Pomodoro Timer");
    ImGui::BulletText("TODO: Sprint 3 - Kanban & Clipboard Manager");
    ImGui::BulletText("TODO: Sprint 4 - File Management Tools");
    ImGui::BulletText("TODO: Sprint 5 - File Conversion & Polish");
}

void SettingsWindow::RenderButtons()
{
    ImGui::Separator();
    ImGui::Spacing();
    
    // Button layout
    float buttonWidth = 80.0f;
    float availWidth = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    
    // Position buttons on the right
    ImGui::SetCursorPosX(availWidth - (buttonWidth * 3 + spacing * 2));
    
    // Apply button
    bool hasChanges = HasUnsavedChanges();
    if (!hasChanges)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    }
    
    if (ImGui::Button("Apply", ImVec2(buttonWidth, 0)) && hasChanges)
    {
        ApplySettings();
    }
    
    if (!hasChanges)
    {
        ImGui::PopStyleVar();
    }
    
    ImGui::SameLine();
    
    // Reset button
    if (ImGui::Button("Reset", ImVec2(buttonWidth, 0)))
    {
        ResetToDefaults();
    }
    
    ImGui::SameLine();
    
    // Close button
    if (ImGui::Button("Close", ImVec2(buttonWidth, 0)))
    {
        if (HasUnsavedChanges())
        {
            ImGui::OpenPopup("UnsavedChanges");
        }
        else
        {
            Hide();
        }
    }
}

void SettingsWindow::LoadCurrentSettings()
{
    AppConfig* config = Application::GetInstance()->GetConfig();
    if (!config) return;
    
    // Load current settings into temp structure
    m_tempSettings.startWithWindows = config->GetValue("app.startWithWindows", false);
    m_tempSettings.minimizeToTray = config->GetValue("app.minimizeToTray", true);
    m_tempSettings.showNotifications = config->GetValue("app.showNotifications", true);
    m_tempSettings.fontSize = config->GetValue("ui.fontSize", 13.0f);
    m_tempSettings.sidebarWidth = config->GetValue("ui.sidebarWidth", 180);
    m_tempSettings.theme = config->GetValue("ui.theme", std::string("dark"));
    m_tempSettings.hotkeyEnabled = config->GetValue("hotkeys.toggleWindow.enabled", true);
    m_tempSettings.hotkeyModifiers = config->GetValue("hotkeys.toggleWindow.modifiers", 6);
    m_tempSettings.hotkeyKey = config->GetValue("hotkeys.toggleWindow.key", 80);
    
    m_settingsChanged = false;
}

void SettingsWindow::ApplySettings()
{
    AppConfig* config = Application::GetInstance()->GetConfig();
    if (!config) return;
    
    // Save temp settings to config
    config->SetValue("app.startWithWindows", m_tempSettings.startWithWindows);
    config->SetValue("app.minimizeToTray", m_tempSettings.minimizeToTray);
    config->SetValue("app.showNotifications", m_tempSettings.showNotifications);
    config->SetValue("ui.fontSize", m_tempSettings.fontSize);
    config->SetValue("ui.sidebarWidth", m_tempSettings.sidebarWidth);
    config->SetValue("ui.theme", m_tempSettings.theme);
    config->SetValue("hotkeys.toggleWindow.enabled", m_tempSettings.hotkeyEnabled);
    config->SetValue("hotkeys.toggleWindow.modifiers", m_tempSettings.hotkeyModifiers);
    config->SetValue("hotkeys.toggleWindow.key", m_tempSettings.hotkeyKey);
    
    // Save to file
    config->Save();
    
    m_settingsChanged = false;
    Logger::Info("Settings applied and saved");
}

void SettingsWindow::ResetToDefaults()
{
    // Reset to default values
    m_tempSettings.startWithWindows = false;
    m_tempSettings.minimizeToTray = true;
    m_tempSettings.showNotifications = true;
    m_tempSettings.fontSize = 13.0f;
    m_tempSettings.sidebarWidth = 180;
    m_tempSettings.theme = "dark";
    m_tempSettings.hotkeyEnabled = true;
    m_tempSettings.hotkeyModifiers = 6; // Ctrl+Shift
    m_tempSettings.hotkeyKey = 80; // P
    
    m_settingsChanged = true;
    Logger::Info("Settings reset to defaults");
}

bool SettingsWindow::HasUnsavedChanges() const
{
    return m_settingsChanged;
}

const char* SettingsWindow::GetCategoryName(SettingsCategory category) const
{
    switch (category)
    {
        case SettingsCategory::General:    return "General";
        case SettingsCategory::Appearance: return "Appearance";
        case SettingsCategory::Hotkeys:    return "Hotkeys";
        case SettingsCategory::About:      return "About";
        default:                           return "Unknown";
    }
}