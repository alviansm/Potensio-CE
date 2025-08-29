#pragma once

#include "DatabaseManager.h"
#include "core/Kanban/KanbanManager.h"
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <sstream>
#include <iomanip>

class KanbanDatabase
{
public:
    KanbanDatabase(std::shared_ptr<DatabaseManager> dbManager);
    ~KanbanDatabase() = default;

    // Database initialization
    bool Initialize();
    
public:
    // Project CRUD operations
    bool CreateProject(const Kanban::Project& project);
    std::optional<std::unique_ptr<Kanban::Project>> GetProject(const std::string& projectId);
    std::vector<std::unique_ptr<Kanban::Project>> GetAllProjects();
    std::vector<std::unique_ptr<Kanban::Project>> GetActiveProjects();
    bool UpdateProject(const Kanban::Project& project);
    bool DeleteProject(const std::string& projectId);
    bool ArchiveProject(const std::string& projectId);
    bool IsProjectExists(const std::string& projectId);    
    
    // Board CRUD operations
    bool CreateBoard(const Kanban::Board& board, const std::string& projectId);
    std::optional<std::unique_ptr<Kanban::Board>> GetBoard(const std::string& boardId);
    std::vector<std::unique_ptr<Kanban::Board>> GetBoardsByProject(const std::string& projectId);
    std::vector<std::unique_ptr<Kanban::Board>> GetAllBoards();
    bool UpdateBoard(const Kanban::Board& board);
    bool DeleteBoard(const std::string& boardId);
    bool ArchiveBoard(const std::string& boardId);
    bool IsBoardExists(const std::string& boardId);
    
    // Column CRUD operations
    bool CreateColumn(const Kanban::Column& column, const std::string& boardId);
    std::optional<std::unique_ptr<Kanban::Column>> GetColumn(const std::string& columnId);
    std::vector<std::unique_ptr<Kanban::Column>> GetColumnsByBoard(const std::string& boardId);
    std::vector<std::unique_ptr<Kanban::Column>> GetAllColumns();
    bool UpdateColumn(const Kanban::Column& column);
    bool DeleteColumn(const std::string& columnId);
    bool UpdateColumnOrder(const std::string& boardId, const std::vector<std::string>& columnIds);
    bool IsColumnExists(const std::string& columnId);
    
    // Card CRUD operations
    bool CreateCard(const Kanban::Card& card, const std::string& columnId);
    std::optional<std::shared_ptr<Kanban::Card>> GetCard(const std::string& cardId);
    std::vector<std::shared_ptr<Kanban::Card>> GetCardsByColumn(const std::string& columnId);
    std::vector<std::shared_ptr<Kanban::Card>> GetCardsByBoard(const std::string& boardId);
    std::vector<std::shared_ptr<Kanban::Card>> GetAllCards();
    std::vector<std::shared_ptr<Kanban::Card>> GetCardsByPriority(Kanban::Priority priority);
    std::vector<std::shared_ptr<Kanban::Card>> GetCardsByStatus(Kanban::CardStatus status);
    std::vector<std::shared_ptr<Kanban::Card>> GetOverdueCards();
    std::vector<std::shared_ptr<Kanban::Card>> GetCardsByAssignee(const std::string& assignee);
    bool UpdateCard(const Kanban::Card& card);
    bool DeleteCard(const std::string& cardId);
    bool UpdateCardOrder(const std::string& columnId, const std::vector<std::string>& cardIds);
    bool MoveCard(const std::string& cardId, const std::string& targetColumnId, int targetIndex = -1);
    bool IsCardExists(const std::string& cardId);
    
    // Tag operations
    bool CreateTag(const std::string& tagName);
    std::vector<std::string> GetAllTags();
    std::vector<std::string> GetCardTags(const std::string& cardId);
    bool AddTagToCard(const std::string& cardId, const std::string& tagName);
    bool RemoveTagFromCard(const std::string& cardId, const std::string& tagName);
    bool UpdateCardTags(const std::string& cardId, const std::vector<std::string>& tags);
    bool DeleteTag(const std::string& tagName);
    
    // Settings operations
    // bool SetSetting(const std::string& key, const std::string& value);
    // std::optional<std::string> GetSetting(const std::string& key);
    // bool DeleteSetting(const std::string& key);
    
    // Utility methods
    std::string GetLastError() const { return m_lastError; }

private:
    std::shared_ptr<DatabaseManager> m_dbManager;
    std::string m_lastError;

    // Schema versioning
    static constexpr int CURRENT_SCHEMA_VERSION = 1;
    
    // Table creation methods
    bool CreateTables();
    bool CreateProjectsTable();
    bool CreateBoardsTable();
    bool CreateColumnsTable();
    bool CreateCardsTable();
    bool CreateTagsTable();
    bool CreateCardsTagsNormalizationTable();
    bool CreateKanbanSettingTable();
    bool CreateIndexes();

    bool MigrateSchema(int fromVersion, int toVersion);
    bool MigrateToVersion1();
    
    // Helper methods for data conversion
    std::string TimePointToString(const std::chrono::system_clock::time_point& tp) const;
    std::chrono::system_clock::time_point StringToTimePoint(const std::string& str) const;
    void SetError(const std::string& error);
    
    // Internal helper methods for loading related data
    bool LoadBoardsForProject(Kanban::Project& project);
    bool LoadColumnsForBoard(Kanban::Board& board);
    bool LoadCardsForColumn(Kanban::Column& column);
    bool LoadTagsForCard(Kanban::Card& card);
    int GetNextOrder(const std::string& parentId, const std::string& orderColumn, const std::string& parentColumn);
};