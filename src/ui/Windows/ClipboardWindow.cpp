// ui/Windows/ClipboardWindow.cpp
#include "ClipboardWindow.h"
#include "core/Clipboard/ClipboardManager.h"
#include "app/AppConfig.h"
#include "core/Logger.h"
#include <imgui.h>
#include <windows.h>
#include <commdlg.h>

ClipboardWindow::ClipboardWindow()
{
}

ClipboardWindow::~ClipboardWindow()
{
    Shutdown();
}

bool ClipboardWindow::Initialize(AppConfig* config)
{
    if (m_isInitialized) return true;

    m_config = config;
    
    // Load UI state from config
    LoadUIState();
    
    m_isInitialized = true;
    Logger::Info("ClipboardWindow initialized successfully");
    return true;
}

void ClipboardWindow::Shutdown()
{
    if (!m_isInitialized) return;
    
    // Save current state before shutdown
    SaveUIState();
    
    m_isInitialized = false;
    Logger::Debug("ClipboardWindow shutdown complete");
}

void ClipboardWindow::Update(float deltaTime)
{
    // Update logic if needed
    (void)deltaTime; // Suppress unused parameter warning
}

void ClipboardWindow::Render()
{
    if (!m_isVisible || !m_isInitialized) return;
    
    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    
    bool isOpen = m_isVisible;
    if (ImGui::Begin("📋 Clipboard Manager Settings", &isOpen, ImGuiWindowFlags_NoCollapse))
    {
        // Tab bar for different settings categories
        if (ImGui::BeginTabBar("ClipboardSettingsTabs"))
        {
            if (ImGui::BeginTabItem("⚙️ General"))
            {
                RenderGeneralSettings();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("📄 Content"))
            {
                RenderContentSettings();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("🖥️ Interface"))
            {
                RenderInterfaceSettings();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("🚫 Exclude Apps"))
            {
                RenderExcludeApps();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("📊 Statistics"))
            {
                RenderStatistics();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("💾 Import/Export"))
            {
                RenderImportExport();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("🔧 Advanced"))
            {
                RenderAdvancedSettings();
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Bottom buttons
        float buttonWidth = 100.0f;
        float totalButtonWidth = 3 * buttonWidth + 2 * 10.0f;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - totalButtonWidth) * 0.5f);
        
        // Apply button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
        
        if (ImGui::Button("Apply", ImVec2(buttonWidth, 0)))
        {
            if (ValidateSettings())
            {
                ApplySettings();
                SaveUIState();
                Logger::Info("Clipboard settings applied successfully");
            }
        }
        ImGui::PopStyleColor(3);
        
        ImGui::SameLine();
        
        // Reset button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.5f, 0.2f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.6f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.4f, 0.1f, 1.0f));
        
        if (ImGui::Button("Reset", ImVec2(buttonWidth, 0)))
        {
            ResetToDefaults();
        }
        ImGui::PopStyleColor(3);
        
        ImGui::SameLine();
        
        // Close button
        if (ImGui::Button("Close", ImVec2(buttonWidth, 0)))
        {
            m_isVisible = false;
        }
    }
    else
    {
        isOpen = false;
    }
    ImGui::End();
    
    if (!isOpen)
    {
        m_isVisible = false;
    }
}

void ClipboardWindow::RenderGeneralSettings()
{
    ImGui::Text("General Settings");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Enable monitoring
    ImGui::Checkbox("Enable clipboard monitoring", &m_uiState.enableMonitoring);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Automatically capture clipboard changes");
    }
    
    ImGui::Spacing();
    
    // History size
    ImGui::Text("Maximum history size:");
    ImGui::SliderInt("##maxHistorySize", &m_uiState.maxHistorySize, 10, 1000, "%d items");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Maximum number of clipboard items to keep in history");
    }
    
    ImGui::Spacing();
    
    // Max item size
    ImGui::Text("Maximum item size:");
    ImGui::SliderInt("##maxItemSize", &m_uiState.maxItemSizeKB, 64, 10240, "%d KB");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Maximum size for a single clipboard item (larger items will be ignored)");
    }
    
    ImGui::Spacing();
    
    // Auto cleanup
    ImGui::Checkbox("Enable automatic cleanup", &m_uiState.autoCleanup);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Automatically remove old clipboard items");
    }
    
    if (m_uiState.autoCleanup)
    {
        ImGui::SameLine();
        ImGui::Text("after");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("##cleanupDays", &m_uiState.autoCleanupDays, 1, 7);
        ImGui::SameLine();
        ImGui::Text("days");
        
        if (m_uiState.autoCleanupDays < 1) m_uiState.autoCleanupDays = 1;
        if (m_uiState.autoCleanupDays > 365) m_uiState.autoCleanupDays = 365;
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Quick actions
    ImGui::Text("Quick Actions");
    ImGui::Spacing();
    
    if (ImGui::Button("Clear History Now", ImVec2(150, 0)))
    {
        if (m_clipboardManager)
        {
            m_clipboardManager->ClearHistory();
            Logger::Info("Clipboard history cleared by user");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cleanup Old Items", ImVec2(150, 0)))
    {
        if (m_clipboardManager)
        {
            m_clipboardManager->ClearOldItems();
            Logger::Info("Old clipboard items cleaned up by user");
        }
    }
}

void ClipboardWindow::RenderContentSettings()
{
    ImGui::Text("Content Types");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Text("Save the following content types to clipboard history:");
    ImGui::Spacing();
    
    // Text content
    ImGui::Checkbox("Plain text", &m_uiState.saveRichText);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Capture plain text and rich text clipboard content");
    }
    
    // Image content
    ImGui::Checkbox("Images", &m_uiState.saveImages);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Capture images copied to clipboard (screenshots, copied images, etc.)");
    }
    
    // File content
    ImGui::Checkbox("Files and folders", &m_uiState.saveFiles);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Capture file paths when files are copied (Ctrl+C in Explorer)");
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Content preview settings
    ImGui::Text("Content Preview");
    ImGui::Spacing();
    
    static bool enablePreviews = true;
    static bool truncateLongText = true;
    static int previewLength = 100;
    
    ImGui::Checkbox("Enable content previews", &enablePreviews);
    
    if (enablePreviews)
    {
        ImGui::Checkbox("Truncate long text", &truncateLongText);
        
        if (truncateLongText)
        {
            ImGui::Text("Preview length:");
            ImGui::SliderInt("##previewLength", &previewLength, 50, 500, "%d characters");
        }
    }
}

void ClipboardWindow::RenderInterfaceSettings()
{
    ImGui::Text("User Interface");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Notifications
    ImGui::Checkbox("Show notifications", &m_uiState.showNotifications);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Show system notifications when new items are captured");
    }
    
    // Hotkeys
    ImGui::Checkbox("Enable hotkeys", &m_uiState.enableHotkeys);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Enable keyboard shortcuts for clipboard operations");
    }
    
    // Monitor when hidden
    ImGui::Checkbox("Monitor when app is hidden", &m_uiState.monitorWhenHidden);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Continue monitoring clipboard when the main window is minimized or hidden");
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Display options
    ImGui::Text("Display Options");
    ImGui::Spacing();
    
    static bool showTimestamps = true;
    static bool showSourceApp = true;
    static bool groupByDate = false;
    static bool showTypeIcons = true;
    
    ImGui::Checkbox("Show timestamps", &showTimestamps);
    ImGui::Checkbox("Show source application", &showSourceApp);
    ImGui::Checkbox("Group items by date", &groupByDate);
    ImGui::Checkbox("Show content type icons", &showTypeIcons);
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Hotkey configuration
    if (m_uiState.enableHotkeys)
    {
        ImGui::Text("Hotkey Configuration");
        ImGui::Spacing();
        
        static char quickPasteHotkey[32] = "Ctrl+Shift+V";
        static char showHistoryHotkey[32] = "Ctrl+Shift+H";
        static char toggleMonitoringHotkey[32] = "Ctrl+Shift+M";
        
        ImGui::Text("Quick paste menu:");
        ImGui::SameLine();
        ImGui::InputText("##quickPaste", quickPasteHotkey, sizeof(quickPasteHotkey));
        
        ImGui::Text("Show clipboard history:");
        ImGui::SameLine();
        ImGui::InputText("##showHistory", showHistoryHotkey, sizeof(showHistoryHotkey));
        
        ImGui::Text("Toggle monitoring:");
        ImGui::SameLine();
        ImGui::InputText("##toggleMonitoring", toggleMonitoringHotkey, sizeof(toggleMonitoringHotkey));
    }
}

void ClipboardWindow::RenderExcludeApps()
{
    ImGui::Text("Exclude Applications");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("Specify applications that should be ignored when monitoring clipboard changes. "
                      "Enter application names separated by commas.");
    ImGui::Spacing();
    
    ImGui::Text("Excluded applications:");
    ImGui::InputTextMultiline("##excludeApps", m_uiState.excludeAppsBuffer, 
                             sizeof(m_uiState.excludeAppsBuffer), ImVec2(-1, 100));
    
    ImGui::Spacing();
    
    if (ImGui::Button("Add Current Application"))
    {
        // Get the currently active window and add it to exclude list
        HWND hwnd = GetForegroundWindow();
        if (hwnd)
        {
            wchar_t title[256];
            if (GetWindowTextW(hwnd, title, sizeof(title) / sizeof(wchar_t)))
            {
                // Convert to UTF-8 and add to exclude list
                int size = WideCharToMultiByte(CP_UTF8, 0, title, -1, nullptr, 0, nullptr, nullptr);
                std::string appName(size - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, title, -1, &appName[0], size, nullptr, nullptr);
                
                std::string currentList = m_uiState.excludeAppsBuffer;
                if (!currentList.empty() && currentList.back() != ',')
                {
                    currentList += ", ";
                }
                currentList += appName;
                
                strncpy_s(m_uiState.excludeAppsBuffer, currentList.c_str(), sizeof(m_uiState.excludeAppsBuffer) - 1);
            }
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Common applications to exclude
    ImGui::Text("Common exclusions:");
    ImGui::Spacing();
    
    if (ImGui::Button("Add Password Manager"))
    {
        std::string currentList = m_uiState.excludeAppsBuffer;
        if (!currentList.empty() && currentList.back() != ',') currentList += ", ";
        currentList += "KeePass, 1Password, LastPass, Bitwarden";
        strncpy_s(m_uiState.excludeAppsBuffer, currentList.c_str(), sizeof(m_uiState.excludeAppsBuffer) - 1);
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Add Development Tools"))
    {
        std::string currentList = m_uiState.excludeAppsBuffer;
        if (!currentList.empty() && currentList.back() != ',') currentList += ", ";
        currentList += "Visual Studio, VS Code, IntelliJ IDEA, Eclipse";
        strncpy_s(m_uiState.excludeAppsBuffer, currentList.c_str(), sizeof(m_uiState.excludeAppsBuffer) - 1);
    }
    
    if (ImGui::Button("Clear All"))
    {
        m_uiState.excludeAppsBuffer[0] = '\0';
    }
}

void ClipboardWindow::RenderStatistics()
{
    ImGui::Text("Clipboard Statistics");
    ImGui::Separator();
    ImGui::Spacing();
    
    if (!m_clipboardManager)
    {
        ImGui::Text("Clipboard manager not available");
        return;
    }
    
    // Basic statistics
    int totalItems = m_clipboardManager->GetTotalItemCount();
    int favoriteItems = m_clipboardManager->GetFavoriteCount();
    size_t totalSize = m_clipboardManager->GetTotalDataSize();
    
    ImGui::Text("Total Items: %d", totalItems);
    ImGui::Text("Favorite Items: %d", favoriteItems);
    
    // Format total size
    std::string sizeStr;
    if (totalSize < 1024)
        sizeStr = std::to_string(totalSize) + " B";
    else if (totalSize < 1024 * 1024)
        sizeStr = std::to_string(totalSize / 1024) + " KB";
    else
        sizeStr = std::to_string(totalSize / (1024 * 1024)) + " MB";
    
    ImGui::Text("Total Size: %s", sizeStr.c_str());
    
    ImGui::Spacing();
    
    // Content type breakdown
    ImGui::Text("Content Types:");
    
    int textItems = m_clipboardManager->GetItemCountByFormat(Clipboard::ClipboardFormat::Text);
    int richTextItems = m_clipboardManager->GetItemCountByFormat(Clipboard::ClipboardFormat::RichText);
    int imageItems = m_clipboardManager->GetItemCountByFormat(Clipboard::ClipboardFormat::Image);
    int fileItems = m_clipboardManager->GetItemCountByFormat(Clipboard::ClipboardFormat::Files);
    
    if (totalItems > 0)
    {
        ImGui::BulletText("Text: %d (%.1f%%)", textItems, (float)textItems / totalItems * 100.0f);
        ImGui::BulletText("Rich Text: %d (%.1f%%)", richTextItems, (float)richTextItems / totalItems * 100.0f);
        ImGui::BulletText("Images: %d (%.1f%%)", imageItems, (float)imageItems / totalItems * 100.0f);
        ImGui::BulletText("Files: %d (%.1f%%)", fileItems, (float)fileItems / totalItems * 100.0f);
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Advanced statistics toggle
    ImGui::Checkbox("Show advanced statistics", &m_uiState.showAdvancedStats);
    
    if (m_uiState.showAdvancedStats)
    {
        ImGui::Spacing();
        ImGui::Text("Advanced Statistics:");
        
        // TODO: Add more detailed statistics
        // - Most active hours
        // - Most copied content
        // - Source applications
        // - Clipboard activity graph
        
        ImGui::Text("Feature coming soon...");
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Advanced analytics will be available in a future update");
    }
}

void ClipboardWindow::RenderImportExport()
{
    ImGui::Text("Import/Export Clipboard History");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Export section
    ImGui::Text("Export History");
    ImGui::Spacing();
    
    ImGui::Text("Export file path:");
    ImGui::InputText("##exportPath", m_uiState.exportPathBuffer, sizeof(m_uiState.exportPathBuffer));
    ImGui::SameLine();
    
    if (ImGui::Button("Browse##export"))
    {
        std::string filePath = ShowSaveFileDialog("Export Clipboard History", "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0");
        if (!filePath.empty())
        {
            strncpy_s(m_uiState.exportPathBuffer, filePath.c_str(), sizeof(m_uiState.exportPathBuffer) - 1);
        }
    }
    
    if (ImGui::Button("Export History", ImVec2(150, 0)))
    {
        if (m_clipboardManager && strlen(m_uiState.exportPathBuffer) > 0)
        {
            if (m_clipboardManager->ExportHistory(m_uiState.exportPathBuffer))
            {
                Logger::Info("Clipboard history exported successfully to: {}", m_uiState.exportPathBuffer);
            }
            else
            {
                Logger::Error("Failed to export clipboard history");
            }
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Import section
    ImGui::Text("Import History");
    ImGui::Spacing();
    
    ImGui::Text("Import file path:");
    ImGui::InputText("##importPath", m_uiState.importPathBuffer, sizeof(m_uiState.importPathBuffer));
    ImGui::SameLine();
    
    if (ImGui::Button("Browse##import"))
    {
        std::string filePath = ShowOpenFileDialog("Import Clipboard History", "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0");
        if (!filePath.empty())
        {
            strncpy_s(m_uiState.importPathBuffer, filePath.c_str(), sizeof(m_uiState.importPathBuffer) - 1);
        }
    }
    
    if (ImGui::Button("Import History", ImVec2(150, 0)))
    {
        if (m_clipboardManager && strlen(m_uiState.importPathBuffer) > 0)
        {
            if (m_clipboardManager->ImportHistory(m_uiState.importPathBuffer))
            {
                Logger::Info("Clipboard history imported successfully from: {}", m_uiState.importPathBuffer);
            }
            else
            {
                Logger::Error("Failed to import clipboard history");
            }
        }
    }
    
    ImGui::Spacing();
    
    ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.2f, 1.0f), "Warning:");
    ImGui::TextWrapped("Importing will merge the imported history with your current clipboard history. "
                      "Consider backing up your current history before importing.");
}

void ClipboardWindow::RenderAdvancedSettings()
{
    ImGui::Text("Advanced Settings");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.2f, 1.0f), "Warning:");
    ImGui::TextWrapped("These settings are for advanced users. Changing them incorrectly may affect clipboard monitoring performance or stability.");
    ImGui::Spacing();
    
    static int monitoringInterval = 100;
    static bool useAlternativeAPI = false;
    static bool enableDebugLogging = false;
    static int maxMemoryUsageMB = 50;
    static bool enableDataCompression = false;
    
    // Performance settings
    ImGui::Text("Performance Settings");
    ImGui::Spacing();
    
    ImGui::Text("Monitoring interval:");
    ImGui::SliderInt("##monitoringInterval", &monitoringInterval, 50, 1000, "%d ms");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("How often to check for clipboard changes (lower = more responsive, higher = less CPU usage)");
    }
    
    ImGui::Text("Maximum memory usage:");
    ImGui::SliderInt("##maxMemory", &maxMemoryUsageMB, 10, 200, "%d MB");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Maximum amount of memory to use for clipboard history");
    }
    
    ImGui::Spacing();
    
    // API settings
    ImGui::Text("System Integration");
    ImGui::Spacing();
    
    ImGui::Checkbox("Use alternative clipboard API", &useAlternativeAPI);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Use an alternative method for clipboard monitoring (may help with compatibility issues)");
    }
    
    ImGui::Checkbox("Enable data compression", &enableDataCompression);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Compress clipboard data to save memory (may slightly increase CPU usage)");
    }
    
    ImGui::Spacing();
    
    // Debug settings
    ImGui::Text("Debugging");
    ImGui::Spacing();
    
    ImGui::Checkbox("Enable debug logging", &enableDebugLogging);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Enable detailed logging for troubleshooting (may affect performance)");
    }
    
    if (enableDebugLogging)
    {
        if (ImGui::Button("Open Log Folder"))
        {
            // TODO: Open log folder
            Logger::Info("Opening log folder...");
        }
    }
}

void ClipboardWindow::LoadUIState()
{
    if (!m_config) return;
    
    // Load from config
    m_uiState.enableMonitoring = m_config->GetValue("clipboard.enable_monitoring", true);
    m_uiState.maxHistorySize = m_config->GetValue("clipboard.max_history_size", 100);
    m_uiState.maxItemSizeKB = m_config->GetValue("clipboard.max_item_size_kb", 1024);
    m_uiState.autoCleanup = m_config->GetValue("clipboard.auto_cleanup", true);
    m_uiState.autoCleanupDays = m_config->GetValue("clipboard.auto_cleanup_days", 30);
    m_uiState.saveImages = m_config->GetValue("clipboard.save_images", true);
    m_uiState.saveFiles = m_config->GetValue("clipboard.save_files", true);
    m_uiState.saveRichText = m_config->GetValue("clipboard.save_rich_text", true);
    m_uiState.showNotifications = m_config->GetValue("clipboard.show_notifications", true);
    m_uiState.enableHotkeys = m_config->GetValue("clipboard.enable_hotkeys", true);
    m_uiState.monitorWhenHidden = m_config->GetValue("clipboard.monitor_when_hidden", true);
    
    std::string excludeApps = m_config->GetValue("clipboard.exclude_apps", std::string(""));
    strncpy_s(m_uiState.excludeAppsBuffer, excludeApps.c_str(), sizeof(m_uiState.excludeAppsBuffer) - 1);
}

void ClipboardWindow::SaveUIState()
{
    if (!m_config) return;
    
    // Save to config
    m_config->SetValue("clipboard.enable_monitoring", m_uiState.enableMonitoring);
    m_config->SetValue("clipboard.max_history_size", m_uiState.maxHistorySize);
    m_config->SetValue("clipboard.max_item_size_kb", m_uiState.maxItemSizeKB);
    m_config->SetValue("clipboard.auto_cleanup", m_uiState.autoCleanup);
    m_config->SetValue("clipboard.auto_cleanup_days", m_uiState.autoCleanupDays);
    m_config->SetValue("clipboard.save_images", m_uiState.saveImages);
    m_config->SetValue("clipboard.save_files", m_uiState.saveFiles);
    m_config->SetValue("clipboard.save_rich_text", m_uiState.saveRichText);
    m_config->SetValue("clipboard.show_notifications", m_uiState.showNotifications);
    m_config->SetValue("clipboard.enable_hotkeys", m_uiState.enableHotkeys);
    m_config->SetValue("clipboard.monitor_when_hidden", m_uiState.monitorWhenHidden);
    m_config->SetValue("clipboard.exclude_apps", std::string(m_uiState.excludeAppsBuffer));
    
    m_config->Save();
}

void ClipboardWindow::ApplySettings()
{
    if (!m_clipboardManager) return;
    
    // Create config object and apply to manager
    Clipboard::ClipboardConfig config;
    config.enableMonitoring = m_uiState.enableMonitoring;
    config.maxHistorySize = m_uiState.maxHistorySize;
    config.maxItemSizeKB = m_uiState.maxItemSizeKB;
    config.autoCleanup = m_uiState.autoCleanup;
    config.autoCleanupDays = m_uiState.autoCleanupDays;
    config.saveImages = m_uiState.saveImages;
    config.saveFiles = m_uiState.saveFiles;
    config.saveRichText = m_uiState.saveRichText;
    config.showNotifications = m_uiState.showNotifications;
    config.enableHotkeys = m_uiState.enableHotkeys;
    config.monitorWhenHidden = m_uiState.monitorWhenHidden;
    config.excludeApps = m_uiState.excludeAppsBuffer;
    
    m_clipboardManager->SetConfig(config);
}

void ClipboardWindow::ResetToDefaults()
{
    m_uiState.enableMonitoring = true;
    m_uiState.maxHistorySize = 100;
    m_uiState.maxItemSizeKB = 1024;
    m_uiState.autoCleanup = true;
    m_uiState.autoCleanupDays = 30;
    m_uiState.saveImages = true;
    m_uiState.saveFiles = true;
    m_uiState.saveRichText = true;
    m_uiState.showNotifications = true;
    m_uiState.enableHotkeys = true;
    m_uiState.monitorWhenHidden = true;
    m_uiState.excludeAppsBuffer[0] = '\0';
    m_uiState.exportPathBuffer[0] = '\0';
    m_uiState.importPathBuffer[0] = '\0';
    m_uiState.showAdvancedStats = false;
}

bool ClipboardWindow::ValidateSettings() const
{
    if (m_uiState.maxHistorySize < 10 || m_uiState.maxHistorySize > 1000)
    {
        ShowValidationError("History size must be between 10 and 1000 items");
        return false;
    }
    
    if (m_uiState.maxItemSizeKB < 64 || m_uiState.maxItemSizeKB > 10240)
    {
        ShowValidationError("Maximum item size must be between 64 KB and 10 MB");
        return false;
    }
    
    if (m_uiState.autoCleanupDays < 1 || m_uiState.autoCleanupDays > 365)
    {
        ShowValidationError("Auto cleanup days must be between 1 and 365");
        return false;
    }
    
    return true;
}

void ClipboardWindow::ShowValidationError(const std::string& message) const
{
    // TODO: Show proper error dialog
    Logger::Warning("Validation error: {}", message);
}

std::string ClipboardWindow::ShowSaveFileDialog(const std::string& title, const std::string& filter)
{
    OPENFILENAME ofn;
    char szFile[260] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.lpstrTitle = title.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    
    if (GetSaveFileName(&ofn))
    {
        return std::string(szFile);
    }
    
    return "";
}

std::string ClipboardWindow::ShowOpenFileDialog(const std::string& title, const std::string& filter)
{
    OPENFILENAME ofn;
    char szFile[260] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.lpstrTitle = title.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (GetOpenFileName(&ofn))
    {
        return std::string(szFile);
    }
    
    return "";
}