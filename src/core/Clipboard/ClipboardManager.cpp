// core/Clipboard/ClipboardManager.cpp
#include "ClipboardManager.h"
#include "app/AppConfig.h"
#include "core/Logger.h"
#include "core/Utils.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <json/json.h>

namespace Clipboard
{
    std::string ClipboardItem::GetFormattedTime() const
    {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    std::string ClipboardItem::GetSizeString() const
    {
        if (dataSize < 1024)
            return std::to_string(dataSize) + " B";
        else if (dataSize < 1024 * 1024)
            return std::to_string(dataSize / 1024) + " KB";
        else
            return std::to_string(dataSize / (1024 * 1024)) + " MB";
    }

    std::string ClipboardItem::GetTypeString() const
    {
        switch (format)
        {
            case ClipboardFormat::Text: return "Text";
            case ClipboardFormat::RichText: return "Rich Text";
            case ClipboardFormat::Image: return "Image";
            case ClipboardFormat::Files: return "Files";
            default: return "Unknown";
        }
    }

    bool ClipboardItem::IsExpired(std::chrono::hours maxAge) const
    {
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - timestamp);
        return age > maxAge;
    }

    bool ClipboardItem::MatchesSearch(const std::string& searchTerm) const
    {
        if (searchTerm.empty()) return true;
        
        std::string lowerSearch = searchTerm;
        std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);
        
        // Search in title
        std::string lowerTitle = title;
        std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), ::tolower);
        if (lowerTitle.find(lowerSearch) != std::string::npos) return true;
        
        // Search in preview
        std::string lowerPreview = preview;
        std::transform(lowerPreview.begin(), lowerPreview.end(), lowerPreview.begin(), ::tolower);
        if (lowerPreview.find(lowerSearch) != std::string::npos) return true;
        
        // Search in content for text items
        if (format == ClipboardFormat::Text || format == ClipboardFormat::RichText)
        {
            std::string lowerContent = content;
            std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::tolower);
            if (lowerContent.find(lowerSearch) != std::string::npos) return true;
        }
        
        return false;
    }
}

ClipboardManager::ClipboardManager()
{
    m_formatFilter = Clipboard::ClipboardFormat::Text; // Default to show all text
}

ClipboardManager::~ClipboardManager()
{
    Shutdown();
}

bool ClipboardManager::Initialize(AppConfig* config)
{
    if (m_isInitialized) return true;
    
    m_config = config;
    
    Logger::Info("Initializing Clipboard Manager...");
    
    // Load configuration
    LoadFromConfig();
    
    // Create hidden window for clipboard monitoring
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"PotensioClipboardWindow";
    
    if (!RegisterClass(&wc))
    {
        Logger::Error("Failed to register clipboard window class");
        return false;
    }
    
    m_hwnd = CreateWindowEx(
        0,
        L"PotensioClipboardWindow",
        L"Potensio Clipboard Monitor",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE, // Message-only window
        nullptr,
        GetModuleHandle(nullptr),
        this
    );
    
    if (!m_hwnd)
    {
        Logger::Error("Failed to create clipboard monitor window");
        return false;
    }
    
    // Load existing history from config
    // TODO: Implement persistent storage
    
    m_isInitialized = true;
    
    // Start monitoring if enabled
    if (m_clipboardConfig.enableMonitoring)
    {
        StartMonitoring();
    }
    
    Logger::Info("Clipboard Manager initialized successfully");
    return true;
}

void ClipboardManager::Shutdown()
{
    if (!m_isInitialized) return;
    
    Logger::Info("Shutting down Clipboard Manager...");
    
    StopMonitoring();
    
    // Save current state
    SaveToConfig();
    
    // Cleanup window
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    
    // Clear data
    m_history.clear();
    m_itemMap.clear();
    
    m_isInitialized = false;
    Logger::Debug("Clipboard Manager shutdown complete");
}

void ClipboardManager::StartMonitoring()
{
    if (!m_isInitialized || m_isMonitoring) return;
    
    // Add to clipboard viewer chain
    m_nextViewer = SetClipboardViewer(m_hwnd);
    m_isMonitoring = true;
    
    Logger::Info("Started clipboard monitoring");
    
    // Capture current clipboard content
    ProcessClipboardChange();
}

void ClipboardManager::StopMonitoring()
{
    if (!m_isMonitoring) return;
    
    // Remove from clipboard viewer chain
    ChangeClipboardChain(m_hwnd, m_nextViewer);
    m_nextViewer = nullptr;
    m_isMonitoring = false;
    
    Logger::Info("Stopped clipboard monitoring");
}

LRESULT CALLBACK ClipboardManager::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_CREATE)
    {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        ClipboardManager* pManager = reinterpret_cast<ClipboardManager*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pManager));
        return 0;
    }
    
    ClipboardManager* pManager = reinterpret_cast<ClipboardManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (pManager)
    {
        return pManager->HandleMessage(hwnd, uMsg, wParam, lParam);
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT ClipboardManager::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_DRAWCLIPBOARD:
            if (!m_ignoreNextChange)
            {
                ProcessClipboardChange();
            }
            m_ignoreNextChange = false;
            
            // Pass message to next viewer
            if (m_nextViewer)
                SendMessage(m_nextViewer, uMsg, wParam, lParam);
            return 0;
            
        case WM_CHANGECBCHAIN:
            if ((HWND)wParam == m_nextViewer)
                m_nextViewer = (HWND)lParam;
            else if (m_nextViewer)
                SendMessage(m_nextViewer, uMsg, wParam, lParam);
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void ClipboardManager::ProcessClipboardChange()
{
    if (!m_isMonitoring) return;
    
    try
    {
        auto item = CreateItemFromClipboard();
        if (item)
        {
            AddItem(item);
            
            if (m_onItemAdded)
                m_onItemAdded(item);
        }
    }
    catch (const std::exception& e)
    {
        Logger::Warning("Error processing clipboard change: {}", e.what());
    }
}

std::shared_ptr<Clipboard::ClipboardItem> ClipboardManager::CreateItemFromClipboard()
{
    if (!OpenClipboard(m_hwnd))
    {
        Logger::Warning("Failed to open clipboard for reading");
        return nullptr;
    }
    
    auto item = std::make_shared<Clipboard::ClipboardItem>();
    item->id = GenerateItemId();
    item->timestamp = std::chrono::system_clock::now();
    item->format = DetectFormat();
    item->source = GetActiveWindowTitle();
    
    switch (item->format)
    {
        case Clipboard::ClipboardFormat::Text:
        case Clipboard::ClipboardFormat::RichText:
            item->content = ExtractTextData();
            item->preview = CreatePreview(item->content, item->format);
            item->title = item->preview.length() > 50 ? 
                         item->preview.substr(0, 47) + "..." : item->preview;
            item->dataSize = item->content.size();
            break;
            
        case Clipboard::ClipboardFormat::Image:
            if (m_clipboardConfig.saveImages)
            {
                item->imageData = ExtractImageData();
                item->dataSize = item->imageData.size();
                item->title = "Image (" + item->GetSizeString() + ")";
                item->preview = "Image from " + item->source;
            }
            break;
            
        case Clipboard::ClipboardFormat::Files:
            if (m_clipboardConfig.saveFiles)
            {
                item->filePaths = ExtractFileData();
                item->dataSize = 0; // Don't store actual file data
                item->title = std::to_string(item->filePaths.size()) + " file(s)";
                item->preview = item->filePaths.empty() ? "" : item->filePaths[0];
                if (item->filePaths.size() > 1)
                    item->preview += " (+" + std::to_string(item->filePaths.size() - 1) + " more)";
            }
            break;
            
        default:
            CloseClipboard();
            return nullptr;
    }
    
    CloseClipboard();
    
    // Check size limits
    if (item->dataSize > static_cast<size_t>(m_clipboardConfig.maxItemSizeKB * 1024))
    {
        Logger::Warning("Clipboard item too large ({} bytes), skipping", item->dataSize);
        return nullptr;
    }
    
    // Check if we should ignore this app
    if (ShouldIgnoreApp(item->source))
    {
        return nullptr;
    }
    
    return item;
}

Clipboard::ClipboardFormat ClipboardManager::DetectFormat() const
{
    // Check in order of preference
    if (IsClipboardFormatAvailable(CF_HDROP))
        return Clipboard::ClipboardFormat::Files;
    
    if (IsClipboardFormatAvailable(CF_BITMAP) || IsClipboardFormatAvailable(CF_DIB))
        return Clipboard::ClipboardFormat::Image;
    
    if (IsClipboardFormatAvailable(CF_UNICODETEXT))
        return Clipboard::ClipboardFormat::Text;
    
    if (IsClipboardFormatAvailable(CF_TEXT))
        return Clipboard::ClipboardFormat::Text;
    
    // Check for rich text format
    UINT rtfFormat = RegisterClipboardFormat(L"Rich Text Format");
    if (IsClipboardFormatAvailable(rtfFormat))
        return Clipboard::ClipboardFormat::RichText;
    
    return Clipboard::ClipboardFormat::Unknown;
}

std::string ClipboardManager::ExtractTextData() const
{
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData)
    {
        hData = GetClipboardData(CF_TEXT);
        if (!hData) return "";
    }
    
    if (hData)
    {
        wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
        if (pszText)
        {
            // Convert to UTF-8
            int size = WideCharToMultiByte(CP_UTF8, 0, pszText, -1, nullptr, 0, nullptr, nullptr);
            std::string result(size - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, pszText, -1, &result[0], size, nullptr, nullptr);
            GlobalUnlock(hData);
            return result;
        }
    }
    
    return "";
}

std::vector<unsigned char> ClipboardManager::ExtractImageData() const
{
    std::vector<unsigned char> result;
    
    HANDLE hData = GetClipboardData(CF_DIB);
    if (hData)
    {
        BITMAPINFO* pBitmapInfo = static_cast<BITMAPINFO*>(GlobalLock(hData));
        if (pBitmapInfo)
        {
            DWORD dataSize = GlobalSize(hData);
            result.resize(dataSize);
            memcpy(result.data(), pBitmapInfo, dataSize);
            GlobalUnlock(hData);
        }
    }
    
    return result;
}

std::vector<std::string> ClipboardManager::ExtractFileData() const
{
    std::vector<std::string> result;
    
    HANDLE hData = GetClipboardData(CF_HDROP);
    if (hData)
    {
        HDROP hDrop = static_cast<HDROP>(hData);
        UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
        
        for (UINT i = 0; i < fileCount; ++i)
        {
            UINT pathLength = DragQueryFile(hDrop, i, nullptr, 0);
            std::wstring wPath(pathLength, 0);
            DragQueryFile(hDrop, i, &wPath[0], pathLength + 1);
            
            // Convert to UTF-8
            int size = WideCharToMultiByte(CP_UTF8, 0, wPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string path(size - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, wPath.c_str(), -1, &path[0], size, nullptr, nullptr);
            
            result.push_back(path);
        }
    }
    
    return result;
}

std::string ClipboardManager::CreatePreview(const std::string& content, Clipboard::ClipboardFormat format) const
{
    if (content.empty()) return "";
    
    std::string preview = content;
    
    // Remove line breaks and extra whitespace for preview
    std::replace(preview.begin(), preview.end(), '\r', ' ');
    std::replace(preview.begin(), preview.end(), '\n', ' ');
    std::replace(preview.begin(), preview.end(), '\t', ' ');
    
    // Remove multiple spaces
    while (preview.find("  ") != std::string::npos)
    {
        preview.replace(preview.find("  "), 2, " ");
    }
    
    // Trim
    preview.erase(0, preview.find_first_not_of(' '));
    preview.erase(preview.find_last_not_of(' ') + 1);
    
    // Limit length
    if (preview.length() > 100)
    {
        preview = preview.substr(0, 97) + "...";
    }
    
    return preview;
}

void ClipboardManager::AddItem(std::shared_ptr<Clipboard::ClipboardItem> item)
{
    if (!item) return;
    
    // Check for duplicates (same content)
    for (auto it = m_history.begin(); it != m_history.end(); ++it)
    {
        if ((*it)->format == item->format && (*it)->content == item->content)
        {
            // Move existing item to front
            auto existingItem = *it;
            m_history.erase(it);
            m_history.insert(m_history.begin(), existingItem);
            existingItem->timestamp = item->timestamp; // Update timestamp
            return;
        }
    }
    
    // Add new item to front
    m_history.insert(m_history.begin(), item);
    m_itemMap[item->id] = item;
    
    // Enforce history limit
    EnforceHistoryLimit();
    
    Logger::Debug("Added clipboard item: {} ({})", item->title, item->GetTypeString());
}

void ClipboardManager::EnforceHistoryLimit()
{
    while (static_cast<int>(m_history.size()) > m_clipboardConfig.maxHistorySize)
    {
        auto lastItem = m_history.back();
        if (!lastItem->isPinned && !lastItem->isFavorite) // Don't remove pinned or favorite items
        {
            m_itemMap.erase(lastItem->id);
            m_history.pop_back();
        }
        else
        {
            // Find first non-pinned, non-favorite item to remove
            bool removed = false;
            for (auto it = m_history.rbegin(); it != m_history.rend(); ++it)
            {
                if (!(*it)->isPinned && !(*it)->isFavorite)
                {
                    m_itemMap.erase((*it)->id);
                    m_history.erase(std::next(it).base());
                    removed = true;
                    break;
                }
            }
            if (!removed) break; // All items are pinned/favorite
        }
    }
}

std::vector<std::shared_ptr<Clipboard::ClipboardItem>> ClipboardManager::GetHistory() const
{
    if (m_searchQuery.empty() && m_formatFilter == Clipboard::ClipboardFormat::Text)
    {
        return m_history; // No filtering needed
    }
    
    std::vector<std::shared_ptr<Clipboard::ClipboardItem>> filtered;
    for (const auto& item : m_history)
    {
        bool matchesSearch = item->MatchesSearch(m_searchQuery);
        bool matchesFormat = (m_formatFilter == Clipboard::ClipboardFormat::Text) || // "All" filter
                            (item->format == m_formatFilter);
        
        if (matchesSearch && matchesFormat)
        {
            filtered.push_back(item);
        }
    }
    
    return filtered;
}

std::vector<std::shared_ptr<Clipboard::ClipboardItem>> ClipboardManager::GetFavorites() const
{
    std::vector<std::shared_ptr<Clipboard::ClipboardItem>> favorites;
    for (const auto& item : m_history)
    {
        if (item->isFavorite)
        {
            favorites.push_back(item);
        }
    }
    return favorites;
}

void ClipboardManager::CopyToClipboard(const std::string& text)
{
    if (!OpenClipboard(m_hwnd))
    {
        Logger::Warning("Failed to open clipboard for writing");
        return;
    }
    
    EmptyClipboard();
    
    // Convert to wide string
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, size * sizeof(wchar_t));
    
    if (hGlobal)
    {
        wchar_t* pGlobal = static_cast<wchar_t*>(GlobalLock(hGlobal));
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, pGlobal, size);
        GlobalUnlock(hGlobal);
        
        m_ignoreNextChange = true; // Don't add our own change to history
        SetClipboardData(CF_UNICODETEXT, hGlobal);
    }
    
    CloseClipboard();
}

void ClipboardManager::CopyToClipboard(std::shared_ptr<Clipboard::ClipboardItem> item)
{
    if (!item) return;
    
    switch (item->format)
    {
        case Clipboard::ClipboardFormat::Text:
        case Clipboard::ClipboardFormat::RichText:
            CopyToClipboard(item->content);
            break;
            
        case Clipboard::ClipboardFormat::Files:
            // TODO: Implement file copying
            Logger::Info("File copying not yet implemented");
            break;
            
        case Clipboard::ClipboardFormat::Image:
            // TODO: Implement image copying
            Logger::Info("Image copying not yet implemented");
            break;
            
        default:
            Logger::Warning("Cannot copy clipboard item of unknown format");
            break;
    }
}

std::string ClipboardManager::GetCurrentClipboardText()
{
    if (!OpenClipboard(m_hwnd))
        return "";
        
    std::string result = ExtractTextData();
    CloseClipboard();
    return result;
}

void ClipboardManager::DeleteItem(const std::string& id)
{
    auto it = std::find_if(m_history.begin(), m_history.end(),
        [&id](const std::shared_ptr<Clipboard::ClipboardItem>& item) {
            return item->id == id;
        });
    
    if (it != m_history.end())
    {
        m_history.erase(it);
        m_itemMap.erase(id);
        
        if (m_onItemDeleted)
            m_onItemDeleted(id);
    }
}

void ClipboardManager::ToggleFavorite(const std::string& id)
{
    auto item = GetItem(id);
    if (item)
    {
        item->isFavorite = !item->isFavorite;
    }
}

void ClipboardManager::TogglePin(const std::string& id)
{
    auto item = GetItem(id);
    if (item)
    {
        item->isPinned = !item->isPinned;
    }
}

std::shared_ptr<Clipboard::ClipboardItem> ClipboardManager::GetItem(const std::string& id) const
{
    auto it = m_itemMap.find(id);
    return (it != m_itemMap.end()) ? it->second : nullptr;
}

void ClipboardManager::ClearHistory()
{
    // Only remove non-pinned, non-favorite items
    auto it = m_history.begin();
    while (it != m_history.end())
    {
        if (!(*it)->isPinned && !(*it)->isFavorite)
        {
            m_itemMap.erase((*it)->id);
            it = m_history.erase(it);
        }
        else
        {
            ++it;
        }
    }
    
    if (m_onHistoryCleared)
        m_onHistoryCleared();
}

void ClipboardManager::LoadFromConfig()
{
    if (!m_config) return;
    
    m_clipboardConfig.enableMonitoring = m_config->GetValue("clipboard.enable_monitoring", true);
    m_clipboardConfig.maxHistorySize = m_config->GetValue("clipboard.max_history_size", 100);
    m_clipboardConfig.maxItemSizeKB = m_config->GetValue("clipboard.max_item_size_kb", 1024);
    m_clipboardConfig.saveImages = m_config->GetValue("clipboard.save_images", true);
    m_clipboardConfig.saveFiles = m_config->GetValue("clipboard.save_files", true);
    m_clipboardConfig.saveRichText = m_config->GetValue("clipboard.save_rich_text", true);
    m_clipboardConfig.autoCleanup = m_config->GetValue("clipboard.auto_cleanup", true);
    m_clipboardConfig.autoCleanupDays = m_config->GetValue("clipboard.auto_cleanup_days", 30);
    m_clipboardConfig.showNotifications = m_config->GetValue("clipboard.show_notifications", true);
    m_clipboardConfig.enableHotkeys = m_config->GetValue("clipboard.enable_hotkeys", true);
    m_clipboardConfig.monitorWhenHidden = m_config->GetValue("clipboard.monitor_when_hidden", true);
    m_clipboardConfig.excludeApps = m_config->GetValue("clipboard.exclude_apps", std::string(""));
}

void ClipboardManager::SaveToConfig() const
{
    if (!m_config) return;
    
    m_config->SetValue("clipboard.enable_monitoring", m_clipboardConfig.enableMonitoring);
    m_config->SetValue("clipboard.max_history_size", m_clipboardConfig.maxHistorySize);
    m_config->SetValue("clipboard.max_item_size_kb", m_clipboardConfig.maxItemSizeKB);
    m_config->SetValue("clipboard.save_images", m_clipboardConfig.saveImages);
    m_config->SetValue("clipboard.save_files", m_clipboardConfig.saveFiles);
    m_config->SetValue("clipboard.save_rich_text", m_clipboardConfig.saveRichText);
    m_config->SetValue("clipboard.auto_cleanup", m_clipboardConfig.autoCleanup);
    m_config->SetValue("clipboard.auto_cleanup_days", m_clipboardConfig.autoCleanupDays);
    m_config->SetValue("clipboard.show_notifications", m_clipboardConfig.showNotifications);
    m_config->SetValue("clipboard.enable_hotkeys", m_clipboardConfig.enableHotkeys);
    m_config->SetValue("clipboard.monitor_when_hidden", m_clipboardConfig.monitorWhenHidden);
    m_config->SetValue("clipboard.exclude_apps", m_clipboardConfig.excludeApps);
    
    m_config->Save();
}

std::string ClipboardManager::GenerateItemId() const
{
    static int counter = 0;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return "clip_" + std::to_string(timestamp) + "_" + std::to_string(++counter);
}

bool ClipboardManager::ShouldIgnoreApp(const std::string& appName) const
{
    if (m_clipboardConfig.excludeApps.empty()) return false;
    
    std::stringstream ss(m_clipboardConfig.excludeApps);
    std::string app;
    
    while (std::getline(ss, app, ','))
    {
        // Trim whitespace
        app.erase(0, app.find_first_not_of(' '));
        app.erase(app.find_last_not_of(' ') + 1);
        
        if (!app.empty() && appName.find(app) != std::string::npos)
        {
            return true;
        }
    }
    
    return false;
}

std::string ClipboardManager::GetActiveWindowTitle() const
{
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return "";
    
    wchar_t title[256];
    if (GetWindowText(hwnd, title, sizeof(title) / sizeof(wchar_t)))
    {
        // Convert to UTF-8
        int size = WideCharToMultiByte(CP_UTF8, 0, title, -1, nullptr, 0, nullptr, nullptr);
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, title, -1, &result[0], size, nullptr, nullptr);
        return result;
    }
    
    return "";
}

// Additional methods for statistics, drag & drop, etc.
int ClipboardManager::GetTotalItemCount() const
{
    return static_cast<int>(m_history.size());
}

int ClipboardManager::GetFavoriteCount() const
{
    return static_cast<int>(std::count_if(m_history.begin(), m_history.end(),
        [](const std::shared_ptr<Clipboard::ClipboardItem>& item) {
            return item->isFavorite;
        }));
}

size_t ClipboardManager::GetTotalDataSize() const
{
    size_t total = 0;
    for (const auto& item : m_history)
    {
        total += item->dataSize;
    }
    return total;
}

void ClipboardManager::SetConfig(const Clipboard::ClipboardConfig& config)
{
    bool wasMonitoring = m_isMonitoring;
    
    if (wasMonitoring && !config.enableMonitoring)
    {
        StopMonitoring();
    }
    
    m_clipboardConfig = config;
    
    if (!wasMonitoring && config.enableMonitoring && m_isInitialized)
    {
        StartMonitoring();
    }
    
    // Enforce new history limit
    EnforceHistoryLimit();
    
    SaveToConfig();
}

void ClipboardManager::StartDrag(std::shared_ptr<Clipboard::ClipboardItem> item, const std::string& category)
{
    m_dragDropState.isDragging = true;
    m_dragDropState.draggedItem = item;
    m_dragDropState.targetCategory = category;
    m_dragDropState.targetIndex = -1;
}

void ClipboardManager::UpdateDrag(const std::string& category, int index)
{
    m_dragDropState.targetCategory = category;
    m_dragDropState.targetIndex = index;
}

void ClipboardManager::EndDrag()
{
    if (m_dragDropState.isDragging && m_dragDropState.draggedItem)
    {
        // TODO: Implement reordering logic if needed
        Logger::Debug("Clipboard drag operation completed");
    }
    
    m_dragDropState = Clipboard::DragDropState();
}

void ClipboardManager::SetSearchQuery(const std::string& query)
{
    m_searchQuery = query;
}

void ClipboardManager::SetFormatFilter(Clipboard::ClipboardFormat format)
{
    m_formatFilter = format;
}