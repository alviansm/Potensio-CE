#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <unordered_map>

// Forward declarations
class AppConfig;

namespace Kanban
{
    enum class Priority
    {
        Low = 0,
        Medium,
        High,
        Urgent
    };

    enum class CardStatus
    {
        Active = 0,
        Completed,
        Archived
    };

    struct Color
    {
        float r, g, b, a = 1.0f;
        
        Color() : r(0.7f), g(0.7f), b(0.7f), a(1.0f) {}
        Color(float red, float green, float blue, float alpha = 1.0f) 
            : r(red), g(green), b(blue), a(alpha) {}
    };

    struct Card
    {
        std::string id;
        std::string title;
        std::string description;
        Priority priority = Priority::Medium;
        CardStatus status = CardStatus::Active;
        Color color;
        std::string assignee;
        std::string dueDate;
        std::vector<std::string> tags;
        std::chrono::system_clock::time_point createdAt;
        std::chrono::system_clock::time_point modifiedAt;
        
        // For drag and drop state
        bool isDragging = false;
        int originalColumnIndex = -1;

        Card();
        Card(const std::string& cardTitle);
        
        // Get priority color
        Color GetPriorityColor() const;
        
        // Get formatted due date
        std::string GetFormattedDueDate() const;
        
        // Check if overdue
        bool IsOverdue() const;
        
        // Generate unique ID
        static std::string GenerateId();
    };

    struct Column
    {
        std::string id;
        std::string name;
        Color headerColor;
        std::vector<std::shared_ptr<Card>> cards;
        int cardLimit = -1; // -1 means no limit
        bool isCollapsed = false;

        Column();
        Column(const std::string& columnName);
        Column(const Column& other)
            : id(other.id),
            name(other.name),
            headerColor(other.headerColor),
            cardLimit(other.cardLimit),
            isCollapsed(other.isCollapsed)
        {
            // std::shared_ptr<Card> is copyable → shallow copy (shared ownership)
            cards = other.cards;
        }

        
        // Card management
        void AddCard(std::shared_ptr<Card> card);
        void RemoveCard(const std::string& cardId);
        std::shared_ptr<Card> FindCard(const std::string& cardId);
        int FindCardIndex(const std::string& cardId);
        
        // Get card count by status/priority
        int GetActiveCardCount() const;
        int GetCompletedCardCount() const;
        int GetUrgentCardCount() const;
    };

    struct Board
    {
        std::string id;
        std::string name;
        std::string description;
        std::vector<std::unique_ptr<Column>> columns;
        std::chrono::system_clock::time_point createdAt;
        std::chrono::system_clock::time_point modifiedAt;
        bool isActive = true;

        Board();
        Board(const std::string& boardName);
        Board(const Board& other)
            : id(other.id),
            name(other.name),
            description(other.description),
            createdAt(other.createdAt),
            modifiedAt(other.modifiedAt),
            isActive(other.isActive)
        {
            columns.reserve(other.columns.size());
            for (const auto& col : other.columns)
            {
                if (col)
                    columns.push_back(std::make_unique<Column>(*col)); // deep copy
            }
        }

        
        // Column management
        void AddColumn(const Kanban::Column& column);
        void AddColumn(const std::string& name);
        void RemoveColumn(const std::string& columnId);
        void MoveColumn(int fromIndex, int toIndex);
        Column* FindColumn(const std::string& columnId);
        int FindColumnIndex(const std::string& columnId);
        
        // Card operations
        std::shared_ptr<Card> FindCard(const std::string& cardId);
        bool MoveCard(const std::string& cardId, const std::string& targetColumnId, int targetIndex = -1);
        
        // Statistics
        int GetTotalCardCount() const;
        int GetCompletedCardCount() const;
        int GetOverdueCardCount() const;
        
        // Default board setup
        void CreateDefaultColumns();
    };

    struct Project
    {
        std::string id;
        std::string name;
        std::string description;
        std::vector<std::unique_ptr<Board>> boards;
        std::chrono::system_clock::time_point createdAt;
        std::chrono::system_clock::time_point modifiedAt;
        bool isActive = true;

        Project();
        Project(const std::string& projectName);
        Project(const Project& other)
            : id(other.id),
            name(other.name),
            description(other.description),
            createdAt(other.createdAt),
            modifiedAt(other.modifiedAt),
            isActive(other.isActive)
        {
            boards.reserve(other.boards.size());
            for (const auto& b : other.boards)
            {
                if (b)
                    boards.push_back(std::make_unique<Board>(*b)); // deep copy
            }
        }

        
        // Board management
        void AddBoard(const std::shared_ptr<Board>& board);
        void AddBoard(const std::string& name);
        void RemoveBoard(const std::string& boardId);
        Board* FindBoard(const std::string& boardId);
        int FindBoardIndex(const std::string& boardId);
        
        // Get active board (first active board)
        Board* GetActiveBoard();
        
        // Statistics
        int GetTotalBoardCount() const;
        int GetTotalCardCount() const;
        int GetCompletedCardCount() const;
    };

    // Drag and drop state
    struct DragDropState
    {
        bool isDragging = false;
        std::shared_ptr<Card> draggedCard = nullptr;
        std::string sourceColumnId;
        int sourceIndex = -1;
        std::string targetColumnId;
        int targetIndex = -1;
        bool isValidDrop = false;
    };
}

class KanbanManager
{
public:
    KanbanManager();
    ~KanbanManager();

    // Initialization
    bool Initialize(AppConfig* config);
    void Shutdown();

    // Project management
    void CreateProject(const std::string& name, const std::string& description = "");
    void DeleteProject(const std::string& projectId);
    Kanban::Project* GetCurrentProject();
    Kanban::Project* FindProject(const std::string& projectId);
    void SetCurrentProject(const std::string& projectId);
    const std::vector<std::unique_ptr<Kanban::Project>>& GetProjects() const { return m_projects; }
    void setCurrentProjects(const std::vector<std::shared_ptr<Kanban::Project>>& projects);

    // Board management
    void CreateBoard(const std::string& name, const std::string& description = "");
    void DeleteBoard(const std::string& boardId);
    Kanban::Board* GetCurrentBoard();
    void SetCurrentBoard(const std::string& boardId);

    // Card operations
    void CreateCard(const std::string& columnId, const std::string& title);
    void DeleteCard(const std::string& cardId);
    void UpdateCard(std::shared_ptr<Kanban::Card> card);
    std::shared_ptr<Kanban::Card> FindCard(const std::string& cardId);

    // Drag and drop
    void StartDrag(std::shared_ptr<Kanban::Card> card, const std::string& sourceColumnId);
    void UpdateDrag(const std::string& targetColumnId, int targetIndex);
    void EndDrag();
    void CancelDrag();
    const Kanban::DragDropState& GetDragDropState() const { return m_dragDropState; }

    // File operations
    bool SaveProject(const std::string& projectId, const std::string& filePath);
    bool LoadProject(const std::string& filePath);
    bool ExportBoard(const std::string& boardId, const std::string& filePath);

    // Configuration
    void SaveSettings();
    void LoadSettings();

    // Statistics
    struct Statistics
    {
        int totalProjects = 0;
        int totalBoards = 0;
        int totalCards = 0;
        int completedCards = 0;
        int overdueCards = 0;
        float completionRate = 0.0f;
    };
    Statistics GetStatistics() const;

    // Callbacks
    void SetOnCardUpdated(std::function<void(std::shared_ptr<Kanban::Card>)> callback) { m_onCardUpdated = callback; }
    void SetOnBoardChanged(std::function<void(Kanban::Board*)> callback) { m_onBoardChanged = callback; }
    void SetOnProjectChanged(std::function<void(Kanban::Project*)> callback) { m_onProjectChanged = callback; }

private:
    // Data
    std::vector<std::unique_ptr<Kanban::Project>> m_projects;
    std::string m_currentProjectId;
    std::string m_currentBoardId;
    
    // Drag and drop state
    Kanban::DragDropState m_dragDropState;
    
    // Configuration
    AppConfig* m_config = nullptr;
    
    // Callbacks
    std::function<void(std::shared_ptr<Kanban::Card>)> m_onCardUpdated;
    std::function<void(Kanban::Board*)> m_onBoardChanged;
    std::function<void(Kanban::Project*)> m_onProjectChanged;
    
    // Helper methods
    void CreateDefaultProject();
    std::string GenerateId() const;
    void NotifyCardUpdated(std::shared_ptr<Kanban::Card> card);
    void NotifyBoardChanged(Kanban::Board* board);
    void NotifyProjectChanged(Kanban::Project* project);
    
    // Persistence helpers
    void LoadDefaultConfiguration();
    void SaveProjectToConfig(const Kanban::Project& project);
    void LoadProjectFromConfig(const std::string& projectId);
};