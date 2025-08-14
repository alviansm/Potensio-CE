// MainWindow.cpp - WITH MENUBAR AND ICON TOOLBAR
#include "MainWindow.h"
#include "ui/Components/Sidebar.h"
#include "core/Logger.h"
#include "core/Utils.h"
#include "app/Application.h"
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
    UnloadTexture(iconCut);
    UnloadTexture(iconPaste);
    UnloadTexture(iconTrash);
    UnloadTexture(iconRename);
    UnloadTexture(iconClear);
    UnloadTexture(iconPin);
    UnloadTexture(iconTrigger);
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
}

void MainWindow::SetCurrentModule(ModulePage module)
{
    m_currentModule = module;
    Logger::Debug("Switched to module: {}", GetModuleName(module));
}

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
            RenderPomodoroPlaceholder();
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

void MainWindow::RenderDropoverInterface()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    
    // Header
    // ImGui::Text("Dropover - File Management");
    // ImGui::Separator();
    // ImGui::Spacing();
    
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
    
    if (ImGui::ImageButton(iconPaste, ImVec2(16, 16))) // 📋 Paste icon
    {
        Logger::Info("Paste Here clicked");
        // TODO: Implement paste from clipboard
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Paste Here");
    
    ImGui::SameLine();
    if (ImGui::ImageButton(iconCut, ImVec2(16, 16))) // ✂ Cut icon
    {
        Logger::Info("Cut Files clicked");
        // TODO: Implement cut selected files
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cut Files");

    ImGui::SameLine();
    if (ImGui::ImageButton(iconRename, ImVec2(16, 16))) // 🔤 Rename icon
    {
        Logger::Info("Bulk Rename clicked");
        // TODO: Open bulk rename dialog
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bulk Rename");
    
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    if (ImGui::ImageButton(iconClear, ImVec2(16, 16))) // 🗑 Clear icon
    {
        Logger::Info("Clear All clicked");
        // TODO: Clear all files from dropover
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear All");
    
    ImGui::SameLine();
    if (ImGui::ImageButton(iconTrash, ImVec2(16, 16))) // 🗑 Trash icon
    {
        Logger::Info("Recycle Bin clicked");
        // TODO: Implement delete selected files
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Recycle Bin");

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    // View options
    if (ImGui::ImageButton(iconPin, ImVec2(16, 16))) // Pin window
    {
        Logger::Info("Pin Window is clicked");
        // TODO: Clear all files from dropover
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pin Window");

    ImGui::SameLine();
    if (ImGui::ImageButton(iconTrigger, ImVec2(16, 16))) // Toggle trigger
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

// Placeholder methods remain the same...
void MainWindow::RenderPomodoroPlaceholder()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    
    ImGui::Text("Pomodoro Timer");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("The Pomodoro Timer will be implemented in Sprint 2.");
    ImGui::Spacing();
    ImGui::Text("Features coming soon:");
    ImGui::BulletText("25-minute focus sessions");
    ImGui::BulletText("5-minute short breaks");
    ImGui::BulletText("15-minute long breaks");
    ImGui::BulletText("Session statistics");
    ImGui::BulletText("Background notifications");
    
    ImGui::PopStyleVar();
}

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