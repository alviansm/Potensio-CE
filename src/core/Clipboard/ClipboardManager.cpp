// core/Clipboard/ClipboardManager.cpp
#include "ClipboardManager.h"
#include "app/AppConfig.h"
#include "core/Logger.h"
#include "core/Utils.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <ctime>

// Windows compatibility defines
#ifndef CF_DIBV5
#define CF_DIBV5 17
#endif

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
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"PotensioClipboardWindow";
    
    if (!RegisterClassW(&wc))
    {
        Logger::Error("Failed to register clipboard window class");
        return false;
    }
    
    m_hwnd = CreateWindowExW(
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
    // Check for files first (highest priority)
    if (IsClipboardFormatAvailable(CF_HDROP))
        return Clipboard::ClipboardFormat::Files;
    
    // Check for images
    if (IsClipboardFormatAvailable(CF_BITMAP) || 
        IsClipboardFormatAvailable(CF_DIB) ||
        IsClipboardFormatAvailable(CF_DIBV5))
        return Clipboard::ClipboardFormat::Image;
    
    // Check for rich text formats
    UINT rtfFormat = RegisterClipboardFormatW(L"Rich Text Format");
    UINT htmlFormat = RegisterClipboardFormatW(L"HTML Format");
    
    if (IsClipboardFormatAvailable(rtfFormat) || IsClipboardFormatAvailable(htmlFormat))
        return Clipboard::ClipboardFormat::RichText;
    
    // Check for Unicode text (preferred)
    if (IsClipboardFormatAvailable(CF_UNICODETEXT))
        return Clipboard::ClipboardFormat::Text;
    
    // Check for ANSI text
    if (IsClipboardFormatAvailable(CF_TEXT))
        return Clipboard::ClipboardFormat::Text;
    
    return Clipboard::ClipboardFormat::Unknown;
}

std::string ClipboardManager::ExtractTextData() const
{
    // Try rich text first
    UINT rtfFormat = RegisterClipboardFormatW(L"Rich Text Format");
    if (IsClipboardFormatAvailable(rtfFormat))
    {
        HANDLE hData = GetClipboardData(rtfFormat);
        if (hData)
        {
            char* pRtfText = static_cast<char*>(GlobalLock(hData));
            if (pRtfText)
            {
                std::string result(pRtfText);
                GlobalUnlock(hData);
                return result;
            }
        }
    }
    
    // Try HTML format
    UINT htmlFormat = RegisterClipboardFormatW(L"HTML Format");
    if (IsClipboardFormatAvailable(htmlFormat))
    {
        HANDLE hData = GetClipboardData(htmlFormat);
        if (hData)
        {
            char* pHtmlText = static_cast<char*>(GlobalLock(hData));
            if (pHtmlText)
            {
                std::string result(pHtmlText);
                GlobalUnlock(hData);
                return result;
            }
        }
    }
    
    // Fall back to Unicode text
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData)
    {
        hData = GetClipboardData(CF_TEXT);
        if (!hData) return "";
    }
    
    if (hData)
    {
        if (IsClipboardFormatAvailable(CF_UNICODETEXT))
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
        else
        {
            char* pszText = static_cast<char*>(GlobalLock(hData));
            if (pszText)
            {
                std::string result(pszText);
                GlobalUnlock(hData);
                return result;
            }
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
            DWORD dataSize = static_cast<DWORD>(GlobalSize(hData));
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
        UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        
        for (UINT i = 0; i < fileCount; ++i)
        {
            UINT pathLength = DragQueryFileW(hDrop, i, nullptr, 0);
            std::wstring wPath(pathLength, 0);
            DragQueryFileW(hDrop, i, &wPath[0], pathLength + 1);
            
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
    if (GetWindowTextW(hwnd, title, sizeof(title) / sizeof(wchar_t)))
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

// Additional missing method implementations
std::vector<std::shared_ptr<Clipboard::ClipboardItem>> ClipboardManager::GetSearchResults(const std::string& query) const
{
    std::vector<std::shared_ptr<Clipboard::ClipboardItem>> results;
    for (const auto& item : m_history)
    {
        if (item->MatchesSearch(query))
        {
            results.push_back(item);
        }
    }
    return results;
}

std::vector<std::shared_ptr<Clipboard::ClipboardItem>> ClipboardManager::GetItemsByFormat(Clipboard::ClipboardFormat format) const
{
    std::vector<std::shared_ptr<Clipboard::ClipboardItem>> results;
    for (const auto& item : m_history)
    {
        if (item->format == format)
        {
            results.push_back(item);
        }
    }
    return results;
}

int ClipboardManager::GetItemCountByFormat(Clipboard::ClipboardFormat format) const
{
    return static_cast<int>(std::count_if(m_history.begin(), m_history.end(),
        [format](const std::shared_ptr<Clipboard::ClipboardItem>& item) {
            return item && item->format == format;
        }));
}

void ClipboardManager::PasteItem(std::shared_ptr<Clipboard::ClipboardItem> item)
{
    if (!item) return;
    
    // Copy to clipboard first
    CopyToClipboard(item);
    
    // Then simulate Ctrl+V paste
    INPUT inputs[4] = {};
    
    // Ctrl down
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    
    // V down
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';
    
    // V up
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    
    // Ctrl up
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    
    SendInput(4, inputs, sizeof(INPUT));
}

void ClipboardManager::ClearOldItems()
{
    if (!m_clipboardConfig.autoCleanup) return;
    
    auto cutoffTime = std::chrono::system_clock::now() - 
                     std::chrono::hours(24 * m_clipboardConfig.autoCleanupDays);
    
    auto it = m_history.begin();
    while (it != m_history.end())
    {
        if ((*it)->timestamp < cutoffTime && !(*it)->isPinned && !(*it)->isFavorite)
        {
            m_itemMap.erase((*it)->id);
            it = m_history.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void ClipboardManager::MoveItem(const std::string& itemId, const std::string& category, int index)
{
    // Simple implementation for reordering items
    auto item = GetItem(itemId);
    if (!item) return;
    
    // Remove from current position
    auto it = std::find_if(m_history.begin(), m_history.end(),
        [&itemId](const std::shared_ptr<Clipboard::ClipboardItem>& item) {
            return item->id == itemId;
        });
    
    if (it != m_history.end())
    {
        m_history.erase(it);
        
        // Insert at new position
        if (index >= 0 && index <= static_cast<int>(m_history.size()))
        {
            m_history.insert(m_history.begin() + index, item);
        }
        else
        {
            m_history.push_back(item); // Add to end if invalid index
        }
    }
}

bool ClipboardManager::ExportHistory(const std::string& filePath) const
{
    try
    {
        std::ofstream file(filePath);
        if (!file.is_open())
        {
            Logger::Error("Failed to open file for export: {}", filePath);
            return false;
        }
        
        // Simple text format with pipe delimiters
        file << "# Potensio Clipboard History Export\n";
        file << "# Version: 1.0\n";
        file << "# Exported at: " << std::time(nullptr) << "\n";
        file << "# Format: ID|Title|Preview|Content|Format|Timestamp|Size|Favorite|Pinned|Source\n";
        file << "#\n";
        
        for (const auto& item : m_history)
        {
            file << EscapeDelimited(item->id) << "|"
                 << EscapeDelimited(item->title) << "|"
                 << EscapeDelimited(item->preview) << "|"
                 << EscapeDelimited(item->content) << "|"
                 << static_cast<int>(item->format) << "|"
                 << std::chrono::duration_cast<std::chrono::seconds>(
                    item->timestamp.time_since_epoch()).count() << "|"
                 << item->dataSize << "|"
                 << (item->isFavorite ? "1" : "0") << "|"
                 << (item->isPinned ? "1" : "0") << "|"
                 << EscapeDelimited(item->source) << "\n";
        }
        
        file.close();
        
        Logger::Info("Exported {} clipboard items to {}", m_history.size(), filePath);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::Error("Exception during export: {}", e.what());
        return false;
    }
}

bool ClipboardManager::ImportHistory(const std::string& filePath)
{
    try
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            Logger::Error("Failed to open file for import: {}", filePath);
            return false;
        }
        
        std::string line;
        int importedCount = 0;
        
        while (std::getline(file, line))
        {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#')
                continue;
            
            // Parse pipe-delimited line
            std::vector<std::string> parts = SplitDelimited(line, '|');
            if (parts.size() < 10)
                continue; // Skip malformed lines
            
            auto item = std::make_shared<Clipboard::ClipboardItem>();
            item->id = parts[0];
            item->title = parts[1];
            item->preview = parts[2];
            item->content = parts[3];
            item->format = static_cast<Clipboard::ClipboardFormat>(std::stoi(parts[4]));
            
            // Convert timestamp
            auto timestamp_seconds = std::stoll(parts[5]);
            item->timestamp = std::chrono::system_clock::from_time_t(timestamp_seconds);
            
            item->dataSize = std::stoull(parts[6]);
            item->isFavorite = (parts[7] == "1");
            item->isPinned = (parts[8] == "1");
            item->source = parts[9];
            
            // Add to history (without triggering callbacks)
            m_history.push_back(item);
            m_itemMap[item->id] = item;
            importedCount++;
        }
        
        file.close();
        
        // Enforce history limit after import
        EnforceHistoryLimit();
        
        Logger::Info("Imported {} clipboard items from {}", importedCount, filePath);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::Error("Exception during import: {}", e.what());
        return false;
    }
}

std::string ClipboardManager::EscapeDelimited(const std::string& input) const
{
    std::string result = input;
    
    // Replace problematic characters
    size_t pos = 0;
    while ((pos = result.find('|', pos)) != std::string::npos)
    {
        result.replace(pos, 1, "\\|");
        pos += 2;
    }
    
    pos = 0;
    while ((pos = result.find('\n', pos)) != std::string::npos)
    {
        result.replace(pos, 1, "\\n");
        pos += 2;
    }
    
    pos = 0;
    while ((pos = result.find('\r', pos)) != std::string::npos)
    {
        result.replace(pos, 1, "\\r");
        pos += 2;
    }
    
    return result;
}

std::vector<std::string> ClipboardManager::SplitDelimited(const std::string& input, char delimiter) const
{
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string item;
    
    while (std::getline(ss, item, delimiter))
    {
        // Unescape characters
        size_t pos = 0;
        while ((pos = item.find("\\|", pos)) != std::string::npos)
        {
            item.replace(pos, 2, "|");
            pos += 1;
        }
        
        pos = 0;
        while ((pos = item.find("\\n", pos)) != std::string::npos)
        {
            item.replace(pos, 2, "\n");
            pos += 1;
        }
        
        pos = 0;
        while ((pos = item.find("\\r", pos)) != std::string::npos)
        {
            item.replace(pos, 2, "\r");
            pos += 1;
        }
        
        result.push_back(item);
    }
    
    return result;
}

void ClipboardManager::PerformAutoCleanup()
{
    if (m_clipboardConfig.autoCleanup)
    {
        ClearOldItems();
    }
}

bool ClipboardManager::IsItemExpired(std::shared_ptr<Clipboard::ClipboardItem> item) const
{
    if (!item || !m_clipboardConfig.autoCleanup) return false;
    
    auto maxAge = std::chrono::hours(24 * m_clipboardConfig.autoCleanupDays);
    return item->IsExpired(maxAge);
}