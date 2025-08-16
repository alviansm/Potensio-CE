// core/Clipboard/ClipboardManager.h
#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <unordered_map>
#include <windows.h>

// Forward declarations
class AppConfig;

namespace Clipboard
{
    enum class ClipboardFormat
    {
        Text = 0,
        RichText,
        Image,
        Files,
        Unknown
    };

    struct ClipboardItem
    {
        std::string id;
        ClipboardFormat format;
        std::string content;        // Text content or file paths
        std::string preview;        // Short preview text
        std::string title;          // Display title
        std::vector<unsigned char> imageData; // For image content
        std::vector<std::string> filePaths;   // For file content
        std::chrono::system_clock::time_point timestamp;
        bool isFavorite = false;
        bool isPinned = false;
        size_t dataSize = 0;        // Size in bytes
        std::string source;         // Source application (if detectable)

        // Helper methods
        std::string GetFormattedTime() const;
        std::string GetSizeString() const;
        std::string GetTypeString() const;
        bool IsExpired(std::chrono::hours maxAge) const;
        bool MatchesSearch(const std::string& searchTerm) const;
    };

    struct ClipboardConfig
    {
        bool enableMonitoring = true;
        int maxHistorySize = 100;
        int maxItemSizeKB = 1024;           // 1MB max per item
        bool saveImages = true;
        bool saveFiles = true;
        bool saveRichText = true;
        bool autoCleanup = true;
        int autoCleanupDays = 30;
        bool showNotifications = true;
        bool enableHotkeys = true;
        bool monitorWhenHidden = true;
        std::string excludeApps = "";       // Comma-separated list
    };

    struct DragDropState
    {
        bool isDragging = false;
        std::shared_ptr<ClipboardItem> draggedItem;
        std::string targetCategory;
        int targetIndex = -1;
    };
}

class ClipboardManager
{
public:
    ClipboardManager();
    ~ClipboardManager();

    // Initialization
    bool Initialize(AppConfig* config);
    void Shutdown();

    // Core functionality
    void StartMonitoring();
    void StopMonitoring();
    bool IsMonitoring() const { return m_isMonitoring; }

    // Clipboard operations
    void CopyToClipboard(const std::string& text);
    void CopyToClipboard(std::shared_ptr<Clipboard::ClipboardItem> item);
    std::string GetCurrentClipboardText();
    void PasteItem(std::shared_ptr<Clipboard::ClipboardItem> item);

    // History management
    std::vector<std::shared_ptr<Clipboard::ClipboardItem>> GetHistory() const;
    std::vector<std::shared_ptr<Clipboard::ClipboardItem>> GetFavorites() const;
    std::vector<std::shared_ptr<Clipboard::ClipboardItem>> GetSearchResults(const std::string& query) const;
    std::vector<std::shared_ptr<Clipboard::ClipboardItem>> GetItemsByFormat(Clipboard::ClipboardFormat format) const;

    // Item operations
    std::shared_ptr<Clipboard::ClipboardItem> GetItem(const std::string& id) const;
    void DeleteItem(const std::string& id);
    void ToggleFavorite(const std::string& id);
    void TogglePin(const std::string& id);
    void ClearHistory();
    void ClearOldItems();

    // Categories and organization
    void MoveItem(const std::string& itemId, const std::string& category, int index = -1);

    // Statistics
    int GetTotalItemCount() const;
    int GetFavoriteCount() const;
    int GetItemCountByFormat(Clipboard::ClipboardFormat format) const;
    size_t GetTotalDataSize() const;

    // Configuration
    void SetConfig(const Clipboard::ClipboardConfig& config);
    Clipboard::ClipboardConfig GetConfig() const { return m_clipboardConfig; }

    // Callbacks
    void SetOnItemAdded(std::function<void(std::shared_ptr<Clipboard::ClipboardItem>)> callback) 
    { 
        m_onItemAdded = callback; 
    }
    
    void SetOnItemDeleted(std::function<void(const std::string&)> callback) 
    { 
        m_onItemDeleted = callback; 
    }
    
    void SetOnHistoryCleared(std::function<void()> callback) 
    { 
        m_onHistoryCleared = callback; 
    }

    // Drag and drop
    void StartDrag(std::shared_ptr<Clipboard::ClipboardItem> item, const std::string& category = "");
    void UpdateDrag(const std::string& category, int index);
    void EndDrag();
    Clipboard::DragDropState& GetDragDropState() { return m_dragDropState; }

    // Search and filter
    void SetSearchQuery(const std::string& query);
    std::string GetSearchQuery() const { return m_searchQuery; }
    void SetFormatFilter(Clipboard::ClipboardFormat format);
    Clipboard::ClipboardFormat GetFormatFilter() const { return m_formatFilter; }

    // Import/Export
    bool ExportHistory(const std::string& filePath) const;
    bool ImportHistory(const std::string& filePath);

private:
    // Configuration and state
    AppConfig* m_config = nullptr;
    Clipboard::ClipboardConfig m_clipboardConfig;
    bool m_isMonitoring = false;
    bool m_isInitialized = false;

    // Clipboard monitoring
    HWND m_hwnd = nullptr;
    HWND m_nextViewer = nullptr;
    bool m_ignoreNextChange = false;

    // Data storage
    std::vector<std::shared_ptr<Clipboard::ClipboardItem>> m_history;
    std::unordered_map<std::string, std::shared_ptr<Clipboard::ClipboardItem>> m_itemMap;

    // Search and filtering
    std::string m_searchQuery;
    Clipboard::ClipboardFormat m_formatFilter = Clipboard::ClipboardFormat::Text;

    // Drag and drop state
    Clipboard::DragDropState m_dragDropState;

    // Callbacks
    std::function<void(std::shared_ptr<Clipboard::ClipboardItem>)> m_onItemAdded;
    std::function<void(const std::string&)> m_onItemDeleted;
    std::function<void()> m_onHistoryCleared;

    // Window message handling
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Internal operations
    void ProcessClipboardChange();
    std::shared_ptr<Clipboard::ClipboardItem> CreateItemFromClipboard();
    void AddItem(std::shared_ptr<Clipboard::ClipboardItem> item);
    void EnforceHistoryLimit();
    bool ShouldIgnoreApp(const std::string& appName) const;
    std::string GetActiveWindowTitle() const;
    
    // Data conversion helpers
    Clipboard::ClipboardFormat DetectFormat() const;
    std::string ExtractTextData() const;
    std::vector<unsigned char> ExtractImageData() const;
    std::vector<std::string> ExtractFileData() const;
    std::string CreatePreview(const std::string& content, Clipboard::ClipboardFormat format) const;
    
    // Persistence
    void LoadFromConfig();
    void SaveToConfig() const;
    std::string GenerateItemId() const;

    // Import/Export helpers
    std::string EscapeDelimited(const std::string& input) const;
    std::vector<std::string> SplitDelimited(const std::string& input, char delimiter) const;

    // Cleanup
    void PerformAutoCleanup();
    bool IsItemExpired(std::shared_ptr<Clipboard::ClipboardItem> item) const;
};