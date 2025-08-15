#include "core/Kanban/KanbanManager.h"
#include "app/AppConfig.h"
#include "core/Logger.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <random>

namespace Kanban
{
    // Card Implementation
    Card::Card()
    {
        id = GenerateId();
        createdAt = std::chrono::system_clock::now();
        modifiedAt = createdAt;
    }

    Card::Card(const std::string& cardTitle) : Card()
    {
        title = cardTitle;
    }

    Color Card::GetPriorityColor() const
    {
        switch (priority)
        {
            case Priority::Low:    return Color(0.5f, 0.8f, 0.5f); // Light green
            case Priority::Medium: return Color(0.8f, 0.8f, 0.5f); // Yellow
            case Priority::High:   return Color(0.9f, 0.6f, 0.3f); // Orange
            case Priority::Urgent: return Color(0.9f, 0.3f, 0.3f); // Red
            default:               return Color(0.7f, 0.7f, 0.7f); // Gray
        }
    }

    std::string Card::GetFormattedDueDate() const
    {
        if (dueDate.empty()) return "";
        return dueDate; // For now, assume dueDate is already formatted
    }

    bool Card::IsOverdue() const
    {
        if (dueDate.empty()) return false;
        // Simplified check - in real implementation, parse dueDate and compare with current time
        auto now = std::chrono::system_clock::now();
        return false; // TODO: Implement proper date comparison
    }

    std::string Card::GenerateId()
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(1000, 9999);
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << "card_" << time_t << "_" << dis(gen);
        return ss.str();
    }

    // Column Implementation
    Column::Column()
    {
        id = "col_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        headerColor = Color(0.3f, 0.5f, 0.8f); // Default blue
    }

    Column::Column(const std::string& columnName) : Column()
    {
        name = columnName;
    }

    void Column::AddCard(std::shared_ptr<Card> card)
    {
        if (card && (cardLimit < 0 || static_cast<int>(cards.size()) < cardLimit))
        {
            cards.push_back(card);
        }
    }

    void Column::RemoveCard(const std::string& cardId)
    {
        cards.erase(
            std::remove_if(cards.begin(), cards.end(),
                [&cardId](const std::shared_ptr<Card>& card) {
                    return card && card->id == cardId;
                }),
            cards.end()
        );
    }

    std::shared_ptr<Card> Column::FindCard(const std::string& cardId)
    {
        auto it = std::find_if(cards.begin(), cards.end(),
            [&cardId](const std::shared_ptr<Card>& card) {
                return card && card->id == cardId;
            });
        
        return (it != cards.end()) ? *it : nullptr;
    }

    int Column::FindCardIndex(const std::string& cardId)
    {
        for (size_t i = 0; i < cards.size(); ++i)
        {
            if (cards[i] && cards[i]->id == cardId)
                return static_cast<int>(i);
        }
        return -1;
    }

    int Column::GetActiveCardCount() const
    {
        return static_cast<int>(std::count_if(cards.begin(), cards.end(),
            [](const std::shared_ptr<Card>& card) {
                return card && card->status == CardStatus::Active;
            }));
    }

    int Column::GetCompletedCardCount() const
    {
        return static_cast<int>(std::count_if(cards.begin(), cards.end(),
            [](const std::shared_ptr<Card>& card) {
                return card && card->status == CardStatus::Completed;
            }));
    }

    int Column::GetUrgentCardCount() const
    {
        return static_cast<int>(std::count_if(cards.begin(), cards.end(),
            [](const std::shared_ptr<Card>& card) {
                return card && card->priority == Priority::Urgent;
            }));
    }

    // Board Implementation
    Board::Board()
    {
        id = "board_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        createdAt = std::chrono::system_clock::now();
        modifiedAt = createdAt;
    }

    Board::Board(const std::string& boardName) : Board()
    {
        name = boardName;
    }

    void Board::AddColumn(const std::string& name)
    {
        auto column = std::make_unique<Column>(name);
        columns.push_back(std::move(column));
        modifiedAt = std::chrono::system_clock::now();
    }

    void Board::RemoveColumn(const std::string& columnId)
    {
        columns.erase(
            std::remove_if(columns.begin(), columns.end(),
                [&columnId](const std::unique_ptr<Column>& col) {
                    return col && col->id == columnId;
                }),
            columns.end()
        );
        modifiedAt = std::chrono::system_clock::now();
    }

    void Board::MoveColumn(int fromIndex, int toIndex)
    {
        if (fromIndex >= 0 && fromIndex < static_cast<int>(columns.size()) &&
            toIndex >= 0 && toIndex < static_cast<int>(columns.size()) &&
            fromIndex != toIndex)
        {
            auto column = std::move(columns[fromIndex]);
            columns.erase(columns.begin() + fromIndex);
            columns.insert(columns.begin() + toIndex, std::move(column));
            modifiedAt = std::chrono::system_clock::now();
        }
    }

    Column* Board::FindColumn(const std::string& columnId)
    {
        auto it = std::find_if(columns.begin(), columns.end(),
            [&columnId](const std::unique_ptr<Column>& col) {
                return col && col->id == columnId;
            });
        
        return (it != columns.end()) ? it->get() : nullptr;
    }

    int Board::FindColumnIndex(const std::string& columnId)
    {
        for (size_t i = 0; i < columns.size(); ++i)
        {
            if (columns[i] && columns[i]->id == columnId)
                return static_cast<int>(i);
        }
        return -1;
    }

    std::shared_ptr<Card> Board::FindCard(const std::string& cardId)
    {
        for (auto& column : columns)
        {
            if (column)
            {
                auto card = column->FindCard(cardId);
                if (card) return card;
            }
        }
        return nullptr;
    }

    bool Board::MoveCard(const std::string& cardId, const std::string& targetColumnId, int targetIndex)
    {
        auto card = FindCard(cardId);
        if (!card) return false;

        auto targetColumn = FindColumn(targetColumnId);
        if (!targetColumn) return false;

        // Remove from source column
        for (auto& column : columns)
        {
            if (column)
            {
                int cardIndex = column->FindCardIndex(cardId);
                if (cardIndex >= 0)
                {
                    column->cards.erase(column->cards.begin() + cardIndex);
                    break;
                }
            }
        }

        // Add to target column
        if (targetIndex < 0 || targetIndex >= static_cast<int>(targetColumn->cards.size()))
        {
            targetColumn->cards.push_back(card);
        }
        else
        {
            targetColumn->cards.insert(targetColumn->cards.begin() + targetIndex, card);
        }

        card->modifiedAt = std::chrono::system_clock::now();
        modifiedAt = std::chrono::system_clock::now();
        return true;
    }

    int Board::GetTotalCardCount() const
    {
        int count = 0;
        for (const auto& column : columns)
        {
            if (column) count += static_cast<int>(column->cards.size());
        }
        return count;
    }

    int Board::GetCompletedCardCount() const
    {
        int count = 0;
        for (const auto& column : columns)
        {
            if (column) count += column->GetCompletedCardCount();
        }
        return count;
    }

    int Board::GetOverdueCardCount() const
    {
        int count = 0;
        for (const auto& column : columns)
        {
            if (column)
            {
                for (const auto& card : column->cards)
                {
                    if (card && card->IsOverdue()) count++;
                }
            }
        }
        return count;
    }

    void Board::CreateDefaultColumns()
    {
        AddColumn("To Do");
        AddColumn("In Progress");
        AddColumn("Review");
        AddColumn("Done");
    }

    // Project Implementation
    Project::Project()
    {
        id = "proj_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        createdAt = std::chrono::system_clock::now();
        modifiedAt = createdAt;
    }

    Project::Project(const std::string& projectName) : Project()
    {
        name = projectName;
    }

    void Project::AddBoard(const std::string& name)
    {
        auto board = std::make_unique<Board>(name);
        board->CreateDefaultColumns();
        boards.push_back(std::move(board));
        modifiedAt = std::chrono::system_clock::now();
    }

    void Project::RemoveBoard(const std::string& boardId)
    {
        boards.erase(
            std::remove_if(boards.begin(), boards.end(),
                [&boardId](const std::unique_ptr<Board>& board) {
                    return board && board->id == boardId;
                }),
            boards.end()
        );
        modifiedAt = std::chrono::system_clock::now();
    }

    Board* Project::FindBoard(const std::string& boardId)
    {
        auto it = std::find_if(boards.begin(), boards.end(),
            [&boardId](const std::unique_ptr<Board>& board) {
                return board && board->id == boardId;
            });
        
        return (it != boards.end()) ? it->get() : nullptr;
    }

    int Project::FindBoardIndex(const std::string& boardId)
    {
        for (size_t i = 0; i < boards.size(); ++i)
        {
            if (boards[i] && boards[i]->id == boardId)
                return static_cast<int>(i);
        }
        return -1;
    }

    Board* Project::GetActiveBoard()
    {
        for (auto& board : boards)
        {
            if (board && board->isActive)
                return board.get();
        }
        return boards.empty() ? nullptr : boards[0].get();
    }

    int Project::GetTotalBoardCount() const
    {
        return static_cast<int>(boards.size());
    }

    int Project::GetTotalCardCount() const
    {
        int count = 0;
        for (const auto& board : boards)
        {
            if (board) count += board->GetTotalCardCount();
        }
        return count;
    }

    int Project::GetCompletedCardCount() const
    {
        int count = 0;
        for (const auto& board : boards)
        {
            if (board) count += board->GetCompletedCardCount();
        }
        return count;
    }
}

// KanbanManager Implementation
KanbanManager::KanbanManager()
{
}

KanbanManager::~KanbanManager()
{
    Shutdown();
}

bool KanbanManager::Initialize(AppConfig* config)
{
    m_config = config;
    
    LoadSettings();
    
    // Create default project if none exist
    if (m_projects.empty())
    {
        CreateDefaultProject();
    }
    
    Logger::Info("KanbanManager initialized successfully");
    return true;
}

void KanbanManager::Shutdown()
{
    SaveSettings();
    
    m_projects.clear();
    m_currentProjectId.clear();
    m_currentBoardId.clear();
    
    Logger::Debug("KanbanManager shutdown complete");
}

void KanbanManager::CreateProject(const std::string& name, const std::string& description)
{
    auto project = std::make_unique<Kanban::Project>(name);
    project->description = description;
    project->AddBoard("Main Board");
    
    m_currentProjectId = project->id;
    if (project->boards.size() > 0)
    {
        m_currentBoardId = project->boards[0]->id;
    }
    
    m_projects.push_back(std::move(project));
    
    Logger::Info("Created project: {}", name);
    NotifyProjectChanged(GetCurrentProject());
}

void KanbanManager::DeleteProject(const std::string& projectId)
{
    auto it = std::find_if(m_projects.begin(), m_projects.end(),
        [&projectId](const std::unique_ptr<Kanban::Project>& proj) {
            return proj && proj->id == projectId;
        });
    
    if (it != m_projects.end())
    {
        std::string name = (*it)->name;
        
        // Update current IDs if we're deleting the current project
        if (m_currentProjectId == projectId)
        {
            m_currentProjectId.clear();
            m_currentBoardId.clear();
            
            // Set to next available project
            if (m_projects.size() > 1)
            {
                for (const auto& proj : m_projects)
                {
                    if (proj && proj->id != projectId)
                    {
                        m_currentProjectId = proj->id;
                        if (!proj->boards.empty())
                        {
                            m_currentBoardId = proj->boards[0]->id;
                        }
                        break;
                    }
                }
            }
        }
        
        m_projects.erase(it);
        Logger::Info("Deleted project: {}", name);
        NotifyProjectChanged(GetCurrentProject());
    }
}

Kanban::Project* KanbanManager::GetCurrentProject()
{
    return FindProject(m_currentProjectId);
}

Kanban::Project* KanbanManager::FindProject(const std::string& projectId)
{
    auto it = std::find_if(m_projects.begin(), m_projects.end(),
        [&projectId](const std::unique_ptr<Kanban::Project>& proj) {
            return proj && proj->id == projectId;
        });
    
    return (it != m_projects.end()) ? it->get() : nullptr;
}

void KanbanManager::SetCurrentProject(const std::string& projectId)
{
    auto project = FindProject(projectId);
    if (project)
    {
        m_currentProjectId = projectId;
        
        // Set current board to first board of the project
        if (!project->boards.empty())
        {
            m_currentBoardId = project->boards[0]->id;
        }
        else
        {
            m_currentBoardId.clear();
        }
        
        Logger::Debug("Switched to project: {}", project->name);
        NotifyProjectChanged(project);
    }
}

void KanbanManager::CreateBoard(const std::string& name, const std::string& description)
{
    auto project = GetCurrentProject();
    if (project)
    {
        auto board = std::make_unique<Kanban::Board>(name);
        board->description = description;
        board->CreateDefaultColumns();
        
        m_currentBoardId = board->id;
        project->boards.push_back(std::move(board));
        
        Logger::Info("Created board: {}", name);
        NotifyBoardChanged(GetCurrentBoard());
    }
}

void KanbanManager::DeleteBoard(const std::string& boardId)
{
    auto project = GetCurrentProject();
    if (project)
    {
        auto board = project->FindBoard(boardId);
        std::string name = board ? board->name : "Unknown";
        
        project->RemoveBoard(boardId);
        
        // Update current board if we deleted it
        if (m_currentBoardId == boardId)
        {
            m_currentBoardId.clear();
            if (!project->boards.empty())
            {
                m_currentBoardId = project->boards[0]->id;
            }
        }
        
        Logger::Info("Deleted board: {}", name);
        NotifyBoardChanged(GetCurrentBoard());
    }
}

Kanban::Board* KanbanManager::GetCurrentBoard()
{
    auto project = GetCurrentProject();
    return project ? project->FindBoard(m_currentBoardId) : nullptr;
}

void KanbanManager::SetCurrentBoard(const std::string& boardId)
{
    auto project = GetCurrentProject();
    if (project && project->FindBoard(boardId))
    {
        m_currentBoardId = boardId;
        Logger::Debug("Switched to board: {}", boardId);
        NotifyBoardChanged(GetCurrentBoard());
    }
}

void KanbanManager::CreateCard(const std::string& columnId, const std::string& title)
{
    auto board = GetCurrentBoard();
    if (board)
    {
        auto column = board->FindColumn(columnId);
        if (column)
        {
            auto card = std::make_shared<Kanban::Card>(title);
            column->AddCard(card);
            
            Logger::Info("Created card: {}", title);
            NotifyCardUpdated(card);
        }
    }
}

void KanbanManager::DeleteCard(const std::string& cardId)
{
    auto board = GetCurrentBoard();
    if (board)
    {
        auto card = board->FindCard(cardId);
        std::string title = card ? card->title : "Unknown";
        
        for (auto& column : board->columns)
        {
            if (column)
            {
                column->RemoveCard(cardId);
            }
        }
        
        Logger::Info("Deleted card: {}", title);
        NotifyBoardChanged(board);
    }
}

void KanbanManager::UpdateCard(std::shared_ptr<Kanban::Card> card)
{
    if (card)
    {
        card->modifiedAt = std::chrono::system_clock::now();
        Logger::Debug("Updated card: {}", card->title);
        NotifyCardUpdated(card);
    }
}

std::shared_ptr<Kanban::Card> KanbanManager::FindCard(const std::string& cardId)
{
    auto board = GetCurrentBoard();
    return board ? board->FindCard(cardId) : nullptr;
}

void KanbanManager::StartDrag(std::shared_ptr<Kanban::Card> card, const std::string& sourceColumnId)
{
    m_dragDropState.isDragging = true;
    m_dragDropState.draggedCard = card;
    m_dragDropState.sourceColumnId = sourceColumnId;
    m_dragDropState.targetColumnId.clear();
    m_dragDropState.sourceIndex = -1;
    m_dragDropState.targetIndex = -1;
    m_dragDropState.isValidDrop = false;
    
    if (card)
    {
        card->isDragging = true;
        Logger::Debug("Started dragging card: {}", card->title);
    }
}

void KanbanManager::UpdateDrag(const std::string& targetColumnId, int targetIndex)
{
    m_dragDropState.targetColumnId = targetColumnId;
    m_dragDropState.targetIndex = targetIndex;
    m_dragDropState.isValidDrop = !targetColumnId.empty();
}

void KanbanManager::EndDrag()
{
    if (m_dragDropState.isDragging && m_dragDropState.draggedCard && m_dragDropState.isValidDrop)
    {
        auto board = GetCurrentBoard();
        if (board)
        {
            bool success = board->MoveCard(
                m_dragDropState.draggedCard->id,
                m_dragDropState.targetColumnId,
                m_dragDropState.targetIndex
            );
            
            if (success)
            {
                Logger::Info("Moved card: {} to column: {}", 
                           m_dragDropState.draggedCard->title,
                           m_dragDropState.targetColumnId);
                NotifyCardUpdated(m_dragDropState.draggedCard);
            }
        }
    }
    
    CancelDrag();
}

void KanbanManager::CancelDrag()
{
    if (m_dragDropState.draggedCard)
    {
        m_dragDropState.draggedCard->isDragging = false;
    }
    
    m_dragDropState = Kanban::DragDropState();
}

bool KanbanManager::SaveProject(const std::string& projectId, const std::string& filePath)
{
    // TODO: Implement XML/JSON export
    Logger::Info("Saving project {} to {}", projectId, filePath);
    return true;
}

bool KanbanManager::LoadProject(const std::string& filePath)
{
    // TODO: Implement XML/JSON import
    Logger::Info("Loading project from {}", filePath);
    return true;
}

bool KanbanManager::ExportBoard(const std::string& boardId, const std::string& filePath)
{
    // TODO: Implement board export
    Logger::Info("Exporting board {} to {}", boardId, filePath);
    return true;
}

void KanbanManager::SaveSettings()
{
    if (!m_config)
        return;
    
    m_config->SetValue("kanban.current_project", m_currentProjectId);
    m_config->SetValue("kanban.current_board", m_currentBoardId);
    
    // Save project count for loading
    m_config->SetValue("kanban.project_count", static_cast<int>(m_projects.size()));
    
    // TODO: Save full project data to config
    m_config->Save();
    
    Logger::Debug("Kanban settings saved");
}

void KanbanManager::LoadSettings()
{
    if (!m_config)
        return;
    
    m_currentProjectId = m_config->GetValue("kanban.current_project", "");
    m_currentBoardId = m_config->GetValue("kanban.current_board", "");
    
    // TODO: Load project data from config
    
    Logger::Debug("Kanban settings loaded");
}

KanbanManager::Statistics KanbanManager::GetStatistics() const
{
    Statistics stats;
    
    stats.totalProjects = static_cast<int>(m_projects.size());
    
    for (const auto& project : m_projects)
    {
        if (project)
        {
            stats.totalBoards += project->GetTotalBoardCount();
            stats.totalCards += project->GetTotalCardCount();
            stats.completedCards += project->GetCompletedCardCount();
        }
    }
    
    // Calculate completion rate
    if (stats.totalCards > 0)
    {
        stats.completionRate = static_cast<float>(stats.completedCards) / stats.totalCards * 100.0f;
    }
    
    return stats;
}

void KanbanManager::CreateDefaultProject()
{
    CreateProject("Default Project", "Default project for getting started");
    
    // Add some sample cards to the first board
    auto board = GetCurrentBoard();
    if (board && !board->columns.empty())
    {
        CreateCard(board->columns[0]->id, "Welcome to Kanban!");
        CreateCard(board->columns[0]->id, "Create your first card");
        
        if (board->columns.size() > 1)
        {
            CreateCard(board->columns[1]->id, "Move cards between columns");
        }
    }
}

std::string KanbanManager::GenerateId() const
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(10000, 99999);
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << time_t << "_" << dis(gen);
    return ss.str();
}

void KanbanManager::NotifyCardUpdated(std::shared_ptr<Kanban::Card> card)
{
    if (m_onCardUpdated && card)
    {
        m_onCardUpdated(card);
    }
}

void KanbanManager::NotifyBoardChanged(Kanban::Board* board)
{
    if (m_onBoardChanged)
    {
        m_onBoardChanged(board);
    }
}

void KanbanManager::NotifyProjectChanged(Kanban::Project* project)
{
    if (m_onProjectChanged)
    {
        m_onProjectChanged(project);
    }
}