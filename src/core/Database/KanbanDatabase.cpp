#include "KanbanDatabase.h"
#include "core/Logger.h"
#include "sqlite3.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>

KanbanDatabase::KanbanDatabase(std::shared_ptr<DatabaseManager> dbManager)
    : m_dbManager(dbManager) {}

bool KanbanDatabase::Initialize() {
  if (!m_dbManager || !m_dbManager->IsConnected()) {
    Logger::Error("KanbanDatabase: Database manager not available");
    return false;
  }

  // Create tables if they don't exist
  if (!CreateTables()) {
    Logger::Error("KanbanDatabase: Failed to create tables");
    return false;
  }

  // Handle schema migrations
  int currentVersion = m_dbManager->GetSchemaVersion();
  if (currentVersion < CURRENT_SCHEMA_VERSION) {
    Logger::Info("KanbanDatabase: Migrating schema from version {} to {}",
                 currentVersion, CURRENT_SCHEMA_VERSION);

    if (!MigrateSchema(currentVersion, CURRENT_SCHEMA_VERSION)) {
      Logger::Error("KanbanDatabase: Schema migration failed");
      return false;
    }

    m_dbManager->SetSchemaVersion(CURRENT_SCHEMA_VERSION);
  }

  Logger::Info("KanbanDatabase initialized successfully");
  return true;
}

bool KanbanDatabase::CreateTables() {
  bool success = true;
  success &= CreateProjectsTable();
  success &= CreateBoardsTable();
  success &= CreateColumnsTable();
  success &= CreateCardsTable();
  success &= CreateTagsTable();
  success &= CreateCardsTagsNormalizationTable();
  success &= CreateKanbanSettingTable();
  success &= CreateIndexes();
  return success;
}

bool KanbanDatabase::MigrateSchema(int fromVersion, int toVersion) {
  return false;
}

bool KanbanDatabase::MigrateToVersion1() { return false; }

bool KanbanDatabase::CreateProjectsTable() 
{ 
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS kanban_projects (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT,
            is_active INTEGER DEFAULT 1,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            modified_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
  )";

  return m_dbManager->ExecuteSQL(sql);
}

bool KanbanDatabase::CreateBoardsTable()
{ 
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS kanban_boards (
            id TEXT PRIMARY KEY,
            project_id TEXT NOT NULL,
            name TEXT NOT NULL,
            description TEXT,
            is_active INTEGER DEFAULT 1,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            modified_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (project_id) REFERENCES kanban_projects(id) ON DELETE CASCADE
        );
  )";

  return m_dbManager->ExecuteSQL(sql);
}

bool KanbanDatabase::CreateColumnsTable() 
{ 
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS kanban_columns (
            id TEXT PRIMARY KEY,
            board_id TEXT NOT NULL,
            name TEXT NOT NULL,
            header_color_r REAL DEFAULT 0.3,
            header_color_g REAL DEFAULT 0.5,
            header_color_b REAL DEFAULT 0.8,
            header_color_a REAL DEFAULT 1.0,
            card_limit INTEGER DEFAULT -1,
            is_collapsed INTEGER DEFAULT 0,
            column_order INTEGER DEFAULT 0,
            FOREIGN KEY (board_id) REFERENCES kanban_boards(id) ON DELETE CASCADE
        );
  )";

  return m_dbManager->ExecuteSQL(sql);
}

bool KanbanDatabase::CreateCardsTable()
{ 
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS kanban_cards (
            id TEXT PRIMARY KEY,
            column_id TEXT NOT NULL,
            title TEXT NOT NULL,
            description TEXT,
            priority INTEGER DEFAULT 1, -- 0=Low, 1=Medium, 2=High, 3=Urgent
            status INTEGER DEFAULT 0,   -- 0=Active, 1=Completed, 2=Archived
            color_r REAL DEFAULT 0.7,
            color_g REAL DEFAULT 0.7,
            color_b REAL DEFAULT 0.7,
            color_a REAL DEFAULT 1.0,
            assignee TEXT,
            due_date DATETIME,
            card_order INTEGER DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            modified_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (column_id) REFERENCES kanban_columns(id) ON DELETE CASCADE
        );
  )";

  return m_dbManager->ExecuteSQL(sql);
}

bool KanbanDatabase::CreateTagsTable() 
{ 
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS kanban_tags (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL
        );
  )";

  return m_dbManager->ExecuteSQL(sql);
}

bool KanbanDatabase::CreateCardsTagsNormalizationTable() 
{ 
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS kanban_card_tags (
            card_id TEXT,
            tag_id INTEGER,
            PRIMARY KEY (card_id, tag_id),
            FOREIGN KEY (card_id) REFERENCES kanban_cards(id) ON DELETE CASCADE,
            FOREIGN KEY (tag_id) REFERENCES kanban_tags(id) ON DELETE CASCADE
        );
  )";

  return m_dbManager->ExecuteSQL(sql);
}

bool KanbanDatabase::CreateKanbanSettingTable() 
{ 
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS kanban_settings (
            key TEXT PRIMARY KEY,
            value TEXT,s
            modified_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
  )";

  return m_dbManager->ExecuteSQL(sql);
}

bool KanbanDatabase::CreateIndexes()
{ 
    const std::string sql = R"(
        CREATE INDEX IF NOT EXISTS idx_kanban_boards_project_id ON kanban_boards(project_id);
        CREATE INDEX IF NOT EXISTS idx_kanban_columns_board_id ON kanban_columns(board_id);
        CREATE INDEX IF NOT EXISTS idx_kanban_cards_column_id ON kanban_cards(column_id);
        CREATE INDEX IF NOT EXISTS idx_kanban_cards_status ON kanban_cards(status);
        CREATE INDEX IF NOT EXISTS idx_kanban_cards_priority ON kanban_cards(priority);
        CREATE INDEX IF NOT EXISTS idx_kanban_cards_due_date ON kanban_cards(due_date);
  )";

  return m_dbManager->ExecuteSQL(sql);
}

// Helper methods for data conversion
std::string KanbanDatabase::TimePointToString(const std::chrono::system_clock::time_point& tp) const
{
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::tm* tm = std::gmtime(&time_t);
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::chrono::system_clock::time_point KanbanDatabase::StringToTimePoint(const std::string& str) const
{
    if (str.empty()) {
        return std::chrono::system_clock::now();
    }
    
    std::tm tm = {};
    std::istringstream iss(str);
    iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    
    if (iss.fail()) {
        return std::chrono::system_clock::now();
    }
    
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

void KanbanDatabase::SetError(const std::string& error)
{
    m_lastError = error;
}

int KanbanDatabase::GetNextOrder(const std::string& parentId, const std::string& orderColumn, const std::string& parentColumn)
{
    int maxOrder = 0;
    std::string sql = "SELECT MAX(" + orderColumn + ") FROM ";
    
    if (orderColumn == "column_order") {
        sql += "kanban_columns WHERE board_id = ?";
    } else if (orderColumn == "card_order") {
        sql += "kanban_cards WHERE column_id = ?";
    } else {
        return 0;
    }

    m_dbManager->ExecuteQuery(sql,
        [&parentId](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, parentId.c_str(), -1, SQLITE_STATIC);
        },
        [&maxOrder](sqlite3_stmt* stmt) -> bool {
            if (!sqlite3_column_type(stmt, 0) == SQLITE_NULL) {
                maxOrder = sqlite3_column_int(stmt, 0);
            }
            return false; // Only get first result
        });
    
    return maxOrder + 1;
}

// Project CRUD operations
bool KanbanDatabase::CreateProject(const Kanban::Project& project)
{
    const std::string sql = R"(
        INSERT INTO kanban_projects (id, name, description, is_active, created_at, modified_at)
        VALUES (?, ?, ?, ?, ?, ?)
    )";

    return m_dbManager->ExecuteSQL(sql, [&project, this](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, project.id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, project.name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, project.description.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 4, project.isActive ? 1 : 0);
        sqlite3_bind_text(stmt, 5, TimePointToString(project.createdAt).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, TimePointToString(project.modifiedAt).c_str(), -1, SQLITE_STATIC);
    });
}

std::optional<std::unique_ptr<Kanban::Project>> KanbanDatabase::GetProject(const std::string& projectId)
{
    std::optional<std::unique_ptr<Kanban::Project>> result;
    
    const std::string sql = R"(
        SELECT id, name, description, is_active, created_at, modified_at 
        FROM kanban_projects WHERE id = ?
    )";

    m_dbManager->ExecuteQuery(sql,
        [&projectId](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, projectId.c_str(), -1, SQLITE_STATIC);
        },
        [&result, this](sqlite3_stmt* stmt) -> bool {
            Kanban::Project project;
            project.id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            project.name = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            project.description = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
            project.isActive = sqlite3_column_int(stmt, 3) == 1;
            project.createdAt = StringToTimePoint(sqlite3_column_text(stmt, 4) ? (char*)sqlite3_column_text(stmt, 4) : "");
            project.modifiedAt = StringToTimePoint(sqlite3_column_text(stmt, 5) ? (char*)sqlite3_column_text(stmt, 5) : "");

            LoadBoardsForProject(project);
            result = std::make_unique<Kanban::Project>(std::move(project));
            return false;
        });

    return result;
}

std::vector<std::unique_ptr<Kanban::Project>> KanbanDatabase::GetAllProjects()
{
    std::vector<std::unique_ptr<Kanban::Project>> projects;
    
    const std::string sql = R"(
        SELECT id, name, description, is_active, created_at, modified_at 
        FROM kanban_projects ORDER BY created_at DESC
    )";

    m_dbManager->ExecuteQuery(sql, [&projects, this](sqlite3_stmt* stmt) -> bool {
        auto project = std::make_unique<Kanban::Project>();
        project->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
        project->name = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
        project->description = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
        project->isActive = sqlite3_column_int(stmt, 3) == 1;
        project->createdAt = StringToTimePoint(sqlite3_column_text(stmt, 4) ? (char*)sqlite3_column_text(stmt, 4) : "");
        project->modifiedAt = StringToTimePoint(sqlite3_column_text(stmt, 5) ? (char*)sqlite3_column_text(stmt, 5) : "");
        
        LoadBoardsForProject(*project);

        // Debug project name and board count
        if (project->boards.empty()) {
            Logger::Debug("Project '{}' has no boards", project->name);
        } else {
            Logger::Debug("Project '{}' has {} boards", project->name, project->boards.size());
        }

        projects.push_back(std::move(project));
        return true;
    });

    return projects;
}

std::vector<std::unique_ptr<Kanban::Project>> KanbanDatabase::GetActiveProjects()
{
    std::vector<std::unique_ptr<Kanban::Project>> projects;
    
    const std::string sql = R"(
        SELECT id, name, description, is_active, created_at, modified_at 
        FROM kanban_projects WHERE is_active = 1 ORDER BY created_at DESC
    )";

    m_dbManager->ExecuteQuery(sql, [&projects, this](sqlite3_stmt* stmt) -> bool {
        Kanban::Project project;
        project.id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
        project.name = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
        project.description = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
        project.isActive = sqlite3_column_int(stmt, 3) == 1;
        project.createdAt = StringToTimePoint(sqlite3_column_text(stmt, 4) ? (char*)sqlite3_column_text(stmt, 4) : "");
        project.modifiedAt = StringToTimePoint(sqlite3_column_text(stmt, 5) ? (char*)sqlite3_column_text(stmt, 5) : "");
        
        LoadBoardsForProject(project);
        projects.push_back(std::make_unique<Kanban::Project>(std::move(project)));
        return true;
    });

    return projects;
}

bool KanbanDatabase::UpdateProject(const Kanban::Project& project)
{
    const std::string sql = R"(
        UPDATE kanban_projects 
        SET name = ?, description = ?, is_active = ?, modified_at = ? 
        WHERE id = ?
    )";

    return m_dbManager->ExecuteSQL(sql, [&project, this](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, project.name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, project.description.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, project.isActive ? 1 : 0);
        sqlite3_bind_text(stmt, 4, TimePointToString(std::chrono::system_clock::now()).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, project.id.c_str(), -1, SQLITE_STATIC);
    });
}

bool KanbanDatabase::DeleteProject(const std::string& projectId)
{
    const std::string sql = "DELETE FROM kanban_projects WHERE id = ?";
    
    return m_dbManager->ExecuteSQL(sql, [&projectId](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, projectId.c_str(), -1, SQLITE_STATIC);
    });
}

bool KanbanDatabase::ArchiveProject(const std::string& projectId)
{
    const std::string sql = R"(
        UPDATE kanban_projects 
        SET is_active = 0, modified_at = ? 
        WHERE id = ?
    )";

    return m_dbManager->ExecuteSQL(sql, [&projectId, this](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, TimePointToString(std::chrono::system_clock::now()).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, projectId.c_str(), -1, SQLITE_STATIC);
    });
}

bool KanbanDatabase::IsProjectExists(const std::string& projectId)
{
    bool exists = false;
    const std::string sql = "SELECT 1 FROM kanban_projects WHERE id = ? LIMIT 1";

    m_dbManager->ExecuteQuery(sql,
        [&projectId](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, projectId.c_str(), -1, SQLITE_STATIC);
        },
        [&exists](sqlite3_stmt* stmt) -> bool {
            exists = sqlite3_column_type(stmt, 0) != SQLITE_NULL;
            return false; // Stop after first row
        });

    return exists;
}

// Board CRUD operations
bool KanbanDatabase::CreateBoard(const Kanban::Board& board, const std::string& projectId)
{
    const std::string sql = R"(
        INSERT INTO kanban_boards (id, project_id, name, description, is_active, created_at, modified_at)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";

    return m_dbManager->ExecuteSQL(sql, [&board, &projectId, this](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, board.id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, projectId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, board.name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, board.description.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 5, board.isActive ? 1 : 0);
        sqlite3_bind_text(stmt, 6, TimePointToString(board.createdAt).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, TimePointToString(board.modifiedAt).c_str(), -1, SQLITE_STATIC);
    });
}

std::optional<std::unique_ptr<Kanban::Board>> KanbanDatabase::GetBoard(const std::string& boardId)
{
    std::optional<std::unique_ptr<Kanban::Board>> result;
    
    const std::string sql = R"(
        SELECT id, name, description, is_active, created_at, modified_at 
        FROM kanban_boards WHERE id = ?
    )";

    m_dbManager->ExecuteQuery(sql,
        [&boardId](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, boardId.c_str(), -1, SQLITE_STATIC);
        },
        [&result, this](sqlite3_stmt* stmt) -> bool {
            Kanban::Board board;
            board.id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            board.name = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            board.description = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
            board.isActive = sqlite3_column_int(stmt, 3) == 1;
            board.createdAt = StringToTimePoint(sqlite3_column_text(stmt, 4) ? (char*)sqlite3_column_text(stmt, 4) : "");
            board.modifiedAt = StringToTimePoint(sqlite3_column_text(stmt, 5) ? (char*)sqlite3_column_text(stmt, 5) : "");

            LoadColumnsForBoard(board);
            result = std::make_unique<Kanban::Board>(std::move(board));

            return false;
        });

    return result;
}

std::vector<std::unique_ptr<Kanban::Board>> KanbanDatabase::GetBoardsByProject(const std::string& projectId)
{
    std::vector<std::unique_ptr<Kanban::Board>> boards;
    
    const std::string sql = R"(
        SELECT id, name, description, is_active, created_at, modified_at 
        FROM kanban_boards WHERE project_id = ? ORDER BY created_at DESC
    )";

    m_dbManager->ExecuteQuery(sql,
        [&projectId](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, projectId.c_str(), -1, SQLITE_STATIC);
        },
        [&boards, this](sqlite3_stmt* stmt) -> bool {
            Kanban::Board board;
            board.id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            board.name = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            board.description = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
            board.isActive = sqlite3_column_int(stmt, 3) == 1;
            board.createdAt = StringToTimePoint(sqlite3_column_text(stmt, 4) ? (char*)sqlite3_column_text(stmt, 4) : "");
            board.modifiedAt = StringToTimePoint(sqlite3_column_text(stmt, 5) ? (char*)sqlite3_column_text(stmt, 5) : "");

            LoadColumnsForBoard(board);
            boards.push_back(std::make_unique<Kanban::Board>(std::move(board)));

            return true; // Continue processing
        });

    return boards;
}

std::vector<std::unique_ptr<Kanban::Board>> KanbanDatabase::GetAllBoards()
{
    std::vector<std::unique_ptr<Kanban::Board>> boards;
    
    const std::string sql = R"(
        SELECT id, name, description, is_active, created_at, modified_at 
        FROM kanban_boards ORDER BY created_at DESC
    )";

    m_dbManager->ExecuteQuery(sql, [&boards, this](sqlite3_stmt* stmt) -> bool {
        auto board = std::unique_ptr<Kanban::Board>();
        board->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
        board->name = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
        board->description = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
        board->isActive = sqlite3_column_int(stmt, 3) == 1;
        board->createdAt = StringToTimePoint(sqlite3_column_text(stmt, 4) ? (char*)sqlite3_column_text(stmt, 4) : "");
        board->modifiedAt = StringToTimePoint(sqlite3_column_text(stmt, 5) ? (char*)sqlite3_column_text(stmt, 5) : "");

        LoadColumnsForBoard(*board);
        boards.push_back(std::move(board));

        return true; // Continue processing
    });

    return boards;
}

bool KanbanDatabase::UpdateBoard(const Kanban::Board& board)
{
    const std::string sql = R"(
        UPDATE kanban_boards 
        SET name = ?, description = ?, is_active = ?, modified_at = ? 
        WHERE id = ?
    )";

    return m_dbManager->ExecuteSQL(sql, [&board, this](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, board.name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, board.description.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, board.isActive ? 1 : 0);
        sqlite3_bind_text(stmt, 4, TimePointToString(std::chrono::system_clock::now()).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, board.id.c_str(), -1, SQLITE_STATIC);
    });
}

bool KanbanDatabase::DeleteBoard(const std::string& boardId)
{
    const std::string sql = "DELETE FROM kanban_boards WHERE id = ?";
    
    return m_dbManager->ExecuteSQL(sql, [&boardId](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, boardId.c_str(), -1, SQLITE_STATIC);
    });
}

bool KanbanDatabase::ArchiveBoard(const std::string& boardId)
{
    const std::string sql = R"(
        UPDATE kanban_boards 
        SET is_active = 0, modified_at = ? 
        WHERE id = ?
    )";

    return m_dbManager->ExecuteSQL(sql, [&boardId, this](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, TimePointToString(std::chrono::system_clock::now()).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, boardId.c_str(), -1, SQLITE_STATIC);
    });
}

bool KanbanDatabase::IsBoardExists(const std::string& boardId)
{
    bool exists = false;
    const std::string sql = "SELECT 1 FROM kanban_boards WHERE id = ? LIMIT 1";

    m_dbManager->ExecuteQuery(sql,
        [&boardId](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, boardId.c_str(), -1, SQLITE_STATIC);
        },
        [&exists](sqlite3_stmt* stmt) -> bool {
            exists = sqlite3_column_type(stmt, 0) != SQLITE_NULL;
            return false; // Stop after first row
        });

    return exists;
}

// Column CRUD operations
bool KanbanDatabase::CreateColumn(const Kanban::Column& column, const std::string& boardId)
{
    int order = GetNextOrder(boardId, "column_order", "board_id");
    
    const std::string sql = R"(
        INSERT INTO kanban_columns (id, board_id, name, header_color_r, header_color_g, header_color_b, header_color_a, 
                                   card_limit, is_collapsed, column_order)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    return m_dbManager->ExecuteSQL(sql, [&column, &boardId, order](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, column.id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, boardId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, column.name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 4, column.headerColor.r);
        sqlite3_bind_double(stmt, 5, column.headerColor.g);
        sqlite3_bind_double(stmt, 6, column.headerColor.b);
        sqlite3_bind_double(stmt, 7, column.headerColor.a);
        sqlite3_bind_int(stmt, 8, column.cardLimit);
        sqlite3_bind_int(stmt, 9, column.isCollapsed ? 1 : 0);
        sqlite3_bind_int(stmt, 10, order);
    });
}

std::optional<std::unique_ptr<Kanban::Column>> KanbanDatabase::GetColumn(const std::string& columnId)
{
    std::optional<std::unique_ptr<Kanban::Column>> result;
    
    const std::string sql = R"(
        SELECT id, name, header_color_r, header_color_g, header_color_b, header_color_a,
               card_limit, is_collapsed
        FROM kanban_columns WHERE id = ?
    )";

    m_dbManager->ExecuteQuery(sql,
        [&columnId](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, columnId.c_str(), -1, SQLITE_STATIC);
        },
        [&result, this](sqlite3_stmt* stmt) -> bool {
            Kanban::Column column;
            column.id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            column.name = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            column.headerColor.r = sqlite3_column_double(stmt, 2);
            column.headerColor.g = sqlite3_column_double(stmt, 3);
            column.headerColor.b = sqlite3_column_double(stmt, 4);
            column.headerColor.a = sqlite3_column_double(stmt, 5);
            column.cardLimit = sqlite3_column_int(stmt, 6);
            column.isCollapsed = sqlite3_column_int(stmt, 7) == 1;
            
            LoadCardsForColumn(column);
            result = std::make_unique<Kanban::Column>(std::move(column));

            return false;
        });

    return result;
}

std::vector<std::unique_ptr<Kanban::Column>> KanbanDatabase::GetColumnsByBoard(const std::string& boardId)
{
    std::vector<std::unique_ptr<Kanban::Column>> columns;
    
    const std::string sql = R"(
        SELECT id, name, header_color_r, header_color_g, header_color_b, header_color_a,
               card_limit, is_collapsed
        FROM kanban_columns WHERE board_id = ? ORDER BY column_order
    )";

    m_dbManager->ExecuteQuery(sql,
        [&boardId](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, boardId.c_str(), -1, SQLITE_STATIC);
        },
        [&columns, this](sqlite3_stmt* stmt) -> bool {
            Kanban::Column column;
            column.id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            column.name = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            column.headerColor.r = sqlite3_column_double(stmt, 2);
            column.headerColor.g = sqlite3_column_double(stmt, 3);
            column.headerColor.b = sqlite3_column_double(stmt, 4);
            column.headerColor.a = sqlite3_column_double(stmt, 5);
            column.cardLimit = sqlite3_column_int(stmt, 6);
            column.isCollapsed = sqlite3_column_int(stmt, 7) == 1;
            
            LoadCardsForColumn(column);
            columns.push_back(std::make_unique<Kanban::Column>(std::move(column)));

            return true;
        });

    return columns;
}

std::vector<std::unique_ptr<Kanban::Column>> KanbanDatabase::GetAllColumns()
{
    std::vector<std::unique_ptr<Kanban::Column>> columns;
    
    const std::string sql = R"(
        SELECT id, name, header_color_r, header_color_g, header_color_b, header_color_a,
               card_limit, is_collapsed
        FROM kanban_columns ORDER BY column_order
    )";

    m_dbManager->ExecuteQuery(sql, [&columns, this](sqlite3_stmt* stmt) -> bool {
        Kanban::Column column;
        column.id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
        column.name = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
        column.headerColor.r = sqlite3_column_double(stmt, 2);
        column.headerColor.g = sqlite3_column_double(stmt, 3);
        column.headerColor.b = sqlite3_column_double(stmt, 4);
        column.headerColor.a = sqlite3_column_double(stmt, 5);
        column.cardLimit = sqlite3_column_int(stmt, 6);
        column.isCollapsed = sqlite3_column_int(stmt, 7) == 1;
        
        LoadCardsForColumn(column);
        columns.push_back(std::make_unique<Kanban::Column>(std::move(column)));
        return true;
    });

    return columns;
}

bool KanbanDatabase::UpdateColumn(const Kanban::Column& column)
{
    const std::string sql = R"(
        UPDATE kanban_columns 
        SET name = ?, header_color_r = ?, header_color_g = ?, header_color_b = ?, header_color_a = ?,
            card_limit = ?, is_collapsed = ?
        WHERE id = ?
    )";

    return m_dbManager->ExecuteSQL(sql, [&column](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, column.name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 2, column.headerColor.r);
        sqlite3_bind_double(stmt, 3, column.headerColor.g);
        sqlite3_bind_double(stmt, 4, column.headerColor.b);
        sqlite3_bind_double(stmt, 5, column.headerColor.a);
        sqlite3_bind_int(stmt, 6, column.cardLimit);
        sqlite3_bind_int(stmt, 7, column.isCollapsed ? 1 : 0);
        sqlite3_bind_text(stmt, 8, column.id.c_str(), -1, SQLITE_STATIC);
    });
}

bool KanbanDatabase::DeleteColumn(const std::string& columnId)
{
    const std::string sql = "DELETE FROM kanban_columns WHERE id = ?";
    
    return m_dbManager->ExecuteSQL(sql, [&columnId](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, columnId.c_str(), -1, SQLITE_STATIC);
    });
}

bool KanbanDatabase::UpdateColumnOrder(const std::string& boardId, const std::vector<std::string>& columnIds)
{
    if (!m_dbManager->BeginTransaction()) {
        return false;
    }

    bool success = true;
    for (size_t i = 0; i < columnIds.size(); ++i) {
        const std::string sql = "UPDATE kanban_columns SET column_order = ? WHERE id = ? AND board_id = ?";
        
        if (!m_dbManager->ExecuteSQL(sql, [&columnIds, i, &boardId](sqlite3_stmt* stmt) {
            sqlite3_bind_int(stmt, 1, static_cast<int>(i));
            sqlite3_bind_text(stmt, 2, columnIds[i].c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, boardId.c_str(), -1, SQLITE_STATIC);
        })) {
            success = false;
            break;
        }
    }

    if (success) {
        m_dbManager->CommitTransaction();
    } else {
        m_dbManager->RollbackTransaction();
    }

    return success;
}

bool KanbanDatabase::IsColumnExists(const std::string& columnId)
{
    bool exists = false;
    const std::string sql = "SELECT 1 FROM kanban_columns WHERE id = ? LIMIT 1";

    m_dbManager->ExecuteQuery(sql,
        [&columnId](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, columnId.c_str(), -1, SQLITE_STATIC);
        },
        [&exists](sqlite3_stmt* stmt) -> bool {
            exists = sqlite3_column_type(stmt, 0) != SQLITE_NULL;
            return false; // Stop after first row
        });

    return exists;
}

// Tag operations
bool KanbanDatabase::CreateTag(const std::string& tagName)
{
    const std::string sql = "INSERT OR IGNORE INTO kanban_tags (name) VALUES (?)";
    
    return m_dbManager->ExecuteSQL(sql, [&tagName](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, tagName.c_str(), -1, SQLITE_STATIC);
    });
}

std::vector<std::string> KanbanDatabase::GetAllTags()
{
    std::vector<std::string> tags;
    
    const std::string sql = "SELECT name FROM kanban_tags ORDER BY name";
    
    m_dbManager->ExecuteQuery(sql, [&tags](sqlite3_stmt* stmt) -> bool {
        const char* tagName = (char*)sqlite3_column_text(stmt, 0);
        if (tagName) {
            tags.emplace_back(tagName);
        }
        return true;
    });

    return tags;
}

std::vector<std::string> KanbanDatabase::GetCardTags(const std::string& cardId)
{
    std::vector<std::string> tags;
    
    const std::string sql = R"(
        SELECT t.name 
        FROM kanban_tags t
        JOIN kanban_card_tags ct ON t.id = ct.tag_id
        WHERE ct.card_id = ?
        ORDER BY t.name
    )";

    m_dbManager->ExecuteQuery(sql,
        [&cardId](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, cardId.c_str(), -1, SQLITE_STATIC);
        },
        [&tags](sqlite3_stmt* stmt) -> bool {
            const char* tagName = (char*)sqlite3_column_text(stmt, 0);
            if (tagName) {
                tags.emplace_back(tagName);
            }
            return true;
        });

    return tags;
}

bool KanbanDatabase::AddTagToCard(const std::string& cardId, const std::string& tagName)
{
    // First ensure the tag exists
    if (!CreateTag(tagName)) {
        return false;
    }

    // Get tag ID
    int tagId = -1;
    const std::string getTagIdSql = "SELECT id FROM kanban_tags WHERE name = ?";
    
    m_dbManager->ExecuteQuery(getTagIdSql,
        [&tagName](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, tagName.c_str(), -1, SQLITE_STATIC);
        },
        [&tagId](sqlite3_stmt* stmt) -> bool {
            tagId = sqlite3_column_int(stmt, 0);
            return false; // Stop after first result
        });

    if (tagId == -1) {
        return false;
    }

    // Add the tag to the card
    const std::string sql = "INSERT OR IGNORE INTO kanban_card_tags (card_id, tag_id) VALUES (?, ?)";
    
    return m_dbManager->ExecuteSQL(sql, [&cardId, tagId](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, cardId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, tagId);
    });
}

bool KanbanDatabase::RemoveTagFromCard(const std::string& cardId, const std::string& tagName)
{
    const std::string sql = R"(
        DELETE FROM kanban_card_tags 
        WHERE card_id = ? AND tag_id = (
            SELECT id FROM kanban_tags WHERE name = ?
        )
    )";
    
    return m_dbManager->ExecuteSQL(sql, [&cardId, &tagName](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, cardId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, tagName.c_str(), -1, SQLITE_STATIC);
    });
}

bool KanbanDatabase::UpdateCardTags(const std::string& cardId, const std::vector<std::string>& tags)
{
    if (!m_dbManager->BeginTransaction()) {
        return false;
    }

    // First, remove all existing tags for this card
    const std::string deleteSql = "DELETE FROM kanban_card_tags WHERE card_id = ?";
    if (!m_dbManager->ExecuteSQL(deleteSql, [&cardId](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, cardId.c_str(), -1, SQLITE_STATIC);
    })) {
        m_dbManager->RollbackTransaction();
        return false;
    }

    // Add each new tag
    bool success = true;
    for (const std::string& tag : tags) {
        if (!AddTagToCard(cardId, tag)) {
            success = false;
            break;
        }
    }

    if (success) {
        m_dbManager->CommitTransaction();
    } else {
        m_dbManager->RollbackTransaction();
    }

    return success;
}

bool KanbanDatabase::DeleteTag(const std::string& tagName)
{
    const std::string sql = "DELETE FROM kanban_tags WHERE name = ?";
    
    return m_dbManager->ExecuteSQL(sql, [&tagName](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, tagName.c_str(), -1, SQLITE_STATIC);
    });
}

// Card CRUD operations
bool KanbanDatabase::CreateCard(const Kanban::Card& card, const std::string& columnId)
{
    int order = GetNextOrder(columnId, "card_order", "column_id");
    
    const std::string sql = R"(
        INSERT INTO kanban_cards (id, column_id, title, description, priority, status,
                                 color_r, color_g, color_b, color_a, assignee, due_date,
                                 card_order, created_at, modified_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    return m_dbManager->ExecuteSQL(sql, [&card, &columnId, order, this](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, card.id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, columnId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, card.title.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, card.description.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 5, static_cast<int>(card.priority));
        sqlite3_bind_int(stmt, 6, static_cast<int>(card.status));
        sqlite3_bind_double(stmt, 7, card.color.r);
        sqlite3_bind_double(stmt, 8, card.color.g);
        sqlite3_bind_double(stmt, 9, card.color.b);
        sqlite3_bind_double(stmt, 10, card.color.a);
        sqlite3_bind_text(stmt, 11, card.assignee.c_str(), -1, SQLITE_STATIC);
        
        if (!card.dueDate.empty()) {
            sqlite3_bind_text(stmt, 12, card.dueDate.c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 12);
        }
        
        sqlite3_bind_int(stmt, 13, order);
        sqlite3_bind_text(stmt, 14, TimePointToString(card.createdAt).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 15, TimePointToString(card.modifiedAt).c_str(), -1, SQLITE_STATIC);
    });
}

std::optional<std::shared_ptr<Kanban::Card>> KanbanDatabase::GetCard(const std::string& cardId)
{
    std::optional<std::shared_ptr<Kanban::Card>> result;
    
    const std::string sql = R"(
        SELECT id, title, description, priority, status, color_r, color_g, color_b, color_a,
               assignee, due_date, created_at, modified_at
        FROM kanban_cards WHERE id = ?
    )";

    m_dbManager->ExecuteQuery(sql,
        [&cardId](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, cardId.c_str(), -1, SQLITE_STATIC);
        },
        [&result, this](sqlite3_stmt* stmt) -> bool {
            Kanban::Card card;
            card.id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            card.title = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            card.description = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
            card.priority = static_cast<Kanban::Priority>(sqlite3_column_int(stmt, 3));
            card.status = static_cast<Kanban::CardStatus>(sqlite3_column_int(stmt, 4));
            card.color.r = sqlite3_column_double(stmt, 5);
            card.color.g = sqlite3_column_double(stmt, 6);
            card.color.b = sqlite3_column_double(stmt, 7);
            card.color.a = sqlite3_column_double(stmt, 8);
            card.assignee = sqlite3_column_text(stmt, 9) ? (char*)sqlite3_column_text(stmt, 9) : "";
            card.dueDate = sqlite3_column_text(stmt, 10) ? (char*)sqlite3_column_text(stmt, 10) : "";
            card.createdAt = StringToTimePoint(sqlite3_column_text(stmt, 11) ? (char*)sqlite3_column_text(stmt, 11) : "");
            card.modifiedAt = StringToTimePoint(sqlite3_column_text(stmt, 12) ? (char*)sqlite3_column_text(stmt, 12) : "");
            
            LoadTagsForCard(card);
            result = std::make_shared<Kanban::Card>(std::move(card));

            return false;
        });

    return result;
}

std::vector<std::shared_ptr<Kanban::Card>> KanbanDatabase::GetCardsByColumn(const std::string& columnId)
{
    std::vector<std::shared_ptr<Kanban::Card>> cards;
    
    const std::string sql = R"(
        SELECT id, title, description, priority, status, color_r, color_g, color_b, color_a,
               assignee, due_date, created_at, modified_at
        FROM kanban_cards WHERE column_id = ? ORDER BY card_order
    )";

    m_dbManager->ExecuteQuery(sql,
        [&columnId](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, columnId.c_str(), -1, SQLITE_STATIC);
        },
        [&cards, this](sqlite3_stmt* stmt) -> bool {
            Kanban::Card card;
            card.id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            card.title = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            card.description = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
            card.priority = static_cast<Kanban::Priority>(sqlite3_column_int(stmt, 3));
            card.status = static_cast<Kanban::CardStatus>(sqlite3_column_int(stmt, 4));
            card.color.r = sqlite3_column_double(stmt, 5);
            card.color.g = sqlite3_column_double(stmt, 6);
            card.color.b = sqlite3_column_double(stmt, 7);
            card.color.a = sqlite3_column_double(stmt, 8);
            card.assignee = sqlite3_column_text(stmt, 9) ? (char*)sqlite3_column_text(stmt, 9) : "";
            card.dueDate = sqlite3_column_text(stmt, 10) ? (char*)sqlite3_column_text(stmt, 10) : "";
            card.createdAt = StringToTimePoint(sqlite3_column_text(stmt, 11) ? (char*)sqlite3_column_text(stmt, 11) : "");
            card.modifiedAt = StringToTimePoint(sqlite3_column_text(stmt, 12) ? (char*)sqlite3_column_text(stmt, 12) : "");
            
            LoadTagsForCard(card);
            cards.push_back(std::make_shared<Kanban::Card>(std::move(card)));
            return true;
        });

    return cards;
}

std::vector<std::shared_ptr<Kanban::Card>> KanbanDatabase::GetCardsByBoard(const std::string& boardId)
{
    std::vector<std::shared_ptr<Kanban::Card>> cards;
    
    const std::string sql = R"(
        SELECT c.id, c.title, c.description, c.priority, c.status, c.color_r, c.color_g, c.color_b, c.color_a,
               c.assignee, c.due_date, c.created_at, c.modified_at
        FROM kanban_cards c
        JOIN kanban_columns col ON c.column_id = col.id
        WHERE col.board_id = ?
        ORDER BY col.column_order, c.card_order
    )";

    m_dbManager->ExecuteQuery(sql,
        [&boardId](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, boardId.c_str(), -1, SQLITE_STATIC);
        },
        [&cards, this](sqlite3_stmt* stmt) -> bool {
            Kanban::Card card;
            card.id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            card.title = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            card.description = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
            card.priority = static_cast<Kanban::Priority>(sqlite3_column_int(stmt, 3));
            card.status = static_cast<Kanban::CardStatus>(sqlite3_column_int(stmt, 4));
            card.color.r = sqlite3_column_double(stmt, 5);
            card.color.g = sqlite3_column_double(stmt, 6);
            card.color.b = sqlite3_column_double(stmt, 7);
            card.color.a = sqlite3_column_double(stmt, 8);
            card.assignee = sqlite3_column_text(stmt, 9) ? (char*)sqlite3_column_text(stmt, 9) : "";
            card.dueDate = sqlite3_column_text(stmt, 10) ? (char*)sqlite3_column_text(stmt, 10) : "";
            card.createdAt = StringToTimePoint(sqlite3_column_text(stmt, 11) ? (char*)sqlite3_column_text(stmt, 11) : "");
            card.modifiedAt = StringToTimePoint(sqlite3_column_text(stmt, 12) ? (char*)sqlite3_column_text(stmt, 12) : "");
            
            LoadTagsForCard(card);
            cards.push_back(std::make_shared<Kanban::Card>(std::move(card)));

            return true;
        });

    return cards;
}

std::vector<std::shared_ptr<Kanban::Card>> KanbanDatabase::GetAllCards()
{
    std::vector<std::shared_ptr<Kanban::Card>> cards;
    
    const std::string sql = R"(
        SELECT id, title, description, priority, status, color_r, color_g, color_b, color_a,
               assignee, due_date, created_at, modified_at
        FROM kanban_cards ORDER BY created_at DESC
    )";

    m_dbManager->ExecuteQuery(sql, [&cards, this](sqlite3_stmt* stmt) -> bool {
        Kanban::Card card;
        card.id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
        card.title = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
        card.description = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
        card.priority = static_cast<Kanban::Priority>(sqlite3_column_int(stmt, 3));
        card.status = static_cast<Kanban::CardStatus>(sqlite3_column_int(stmt, 4));
        card.color.r = sqlite3_column_double(stmt, 5);
        card.color.g = sqlite3_column_double(stmt, 6);
        card.color.b = sqlite3_column_double(stmt, 7);
        card.color.a = sqlite3_column_double(stmt, 8);
        card.assignee = sqlite3_column_text(stmt, 9) ? (char*)sqlite3_column_text(stmt, 9) : "";
        card.dueDate = sqlite3_column_text(stmt, 10) ? (char*)sqlite3_column_text(stmt, 10) : "";
        card.createdAt = StringToTimePoint(sqlite3_column_text(stmt, 11) ? (char*)sqlite3_column_text(stmt, 11) : "");
        card.modifiedAt = StringToTimePoint(sqlite3_column_text(stmt, 12) ? (char*)sqlite3_column_text(stmt, 12) : "");
        
        LoadTagsForCard(card);
        cards.push_back(std::make_shared<Kanban::Card>(std::move(card)));

        return true;
    });

    return cards;
}

std::vector<std::shared_ptr<Kanban::Card>> KanbanDatabase::GetCardsByPriority(Kanban::Priority priority)
{
    std::vector<std::shared_ptr<Kanban::Card>> cards;
    
    const std::string sql = R"(
        SELECT id, title, description, priority, status, color_r, color_g, color_b, color_a,
               assignee, due_date, created_at, modified_at
        FROM kanban_cards WHERE priority = ? ORDER BY created_at DESC
    )";

    m_dbManager->ExecuteQuery(sql,
        [&priority](sqlite3_stmt* stmt) {
            sqlite3_bind_int(stmt, 1, static_cast<int>(priority));
        },
        [&cards, this](sqlite3_stmt* stmt) -> bool {
            Kanban::Card card;
            card.id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            card.title = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            card.description = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
            card.priority = static_cast<Kanban::Priority>(sqlite3_column_int(stmt, 3));
            card.status = static_cast<Kanban::CardStatus>(sqlite3_column_int(stmt, 4));
            card.color.r = sqlite3_column_double(stmt, 5);
            card.color.g = sqlite3_column_double(stmt, 6);
            card.color.b = sqlite3_column_double(stmt, 7);
            card.color.a = sqlite3_column_double(stmt, 8);
            card.assignee = sqlite3_column_text(stmt, 9) ? (char*)sqlite3_column_text(stmt, 9) : "";
            card.dueDate = sqlite3_column_text(stmt, 10) ? (char*)sqlite3_column_text(stmt, 10) : "";
            card.createdAt = StringToTimePoint(sqlite3_column_text(stmt, 11) ? (char*)sqlite3_column_text(stmt, 11) : "");
            card.modifiedAt = StringToTimePoint(sqlite3_column_text(stmt, 12) ? (char*)sqlite3_column_text(stmt, 12) : "");
            
            LoadTagsForCard(card);
            cards.push_back(std::make_shared<Kanban::Card>(std::move(card)));
            return true;
        });

    return cards;
}

std::vector<std::shared_ptr<Kanban::Card>> KanbanDatabase::GetCardsByStatus(Kanban::CardStatus status)
{
    std::vector<std::shared_ptr<Kanban::Card>> cards;
    
    const std::string sql = R"(
        SELECT id, title, description, priority, status, color_r, color_g, color_b, color_a,
               assignee, due_date, created_at, modified_at
        FROM kanban_cards WHERE status = ? ORDER BY created_at DESC
    )";

    m_dbManager->ExecuteQuery(sql,
        [&status](sqlite3_stmt* stmt) {
            sqlite3_bind_int(stmt, 1, static_cast<int>(status));
        },
        [&cards, this](sqlite3_stmt* stmt) -> bool {
            Kanban::Card card;
            card.id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            card.title = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            card.description = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
            card.priority = static_cast<Kanban::Priority>(sqlite3_column_int(stmt, 3));
            card.status = static_cast<Kanban::CardStatus>(sqlite3_column_int(stmt, 4));
            card.color.r = sqlite3_column_double(stmt, 5);
            card.color.g = sqlite3_column_double(stmt, 6);
            card.color.b = sqlite3_column_double(stmt, 7);
            card.color.a = sqlite3_column_double(stmt, 8);
            card.assignee = sqlite3_column_text(stmt, 9) ? (char*)sqlite3_column_text(stmt, 9) : "";
            card.dueDate = sqlite3_column_text(stmt, 10) ? (char*)sqlite3_column_text(stmt, 10) : "";
            card.createdAt = StringToTimePoint(sqlite3_column_text(stmt, 11) ? (char*)sqlite3_column_text(stmt, 11) : "");
            card.modifiedAt = StringToTimePoint(sqlite3_column_text(stmt, 12) ? (char*)sqlite3_column_text(stmt, 12) : "");
            
            LoadTagsForCard(card);
            cards.push_back(std::make_shared<Kanban::Card>(std::move(card)));

            return true;
        });

    return cards;
}

std::vector<std::shared_ptr<Kanban::Card>> KanbanDatabase::GetOverdueCards()
{
    std::vector<std::shared_ptr<Kanban::Card>> cards;
    
    const std::string sql = R"(
        SELECT id, title, description, priority, status, color_r, color_g, color_b, color_a,
               assignee, due_date, created_at, modified_at
        FROM kanban_cards 
        WHERE due_date IS NOT NULL AND due_date < CURRENT_TIMESTAMP AND status = 0
        ORDER BY due_date ASC
    )";

    m_dbManager->ExecuteQuery(sql, [&cards, this](sqlite3_stmt* stmt) -> bool {
        Kanban::Card card;
        card.id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
        card.title = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
        card.description = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
        card.priority = static_cast<Kanban::Priority>(sqlite3_column_int(stmt, 3));
        card.status = static_cast<Kanban::CardStatus>(sqlite3_column_int(stmt, 4));
        card.color.r = sqlite3_column_double(stmt, 5);
        card.color.g = sqlite3_column_double(stmt, 6);
        card.color.b = sqlite3_column_double(stmt, 7);
        card.color.a = sqlite3_column_double(stmt, 8);
        card.assignee = sqlite3_column_text(stmt, 9) ? (char*)sqlite3_column_text(stmt, 9) : "";
        card.dueDate = sqlite3_column_text(stmt, 10) ? (char*)sqlite3_column_text(stmt, 10) : "";
        card.createdAt = StringToTimePoint(sqlite3_column_text(stmt, 11) ? (char*)sqlite3_column_text(stmt, 11) : "");
        card.modifiedAt = StringToTimePoint(sqlite3_column_text(stmt, 12) ? (char*)sqlite3_column_text(stmt, 12) : "");
        
        LoadTagsForCard(card);
        cards.push_back(std::make_shared<Kanban::Card>(std::move(card)));

        return true;
    });

    return cards;
}

std::vector<std::shared_ptr<Kanban::Card>> KanbanDatabase::GetCardsByAssignee(const std::string& assignee)
{
    std::vector<std::shared_ptr<Kanban::Card>> cards;
    
    const std::string sql = R"(
        SELECT id, title, description, priority, status, color_r, color_g, color_b, color_a,
               assignee, due_date, created_at, modified_at
        FROM kanban_cards WHERE assignee = ? ORDER BY created_at DESC
    )";

    m_dbManager->ExecuteQuery(sql,
        [&assignee](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, assignee.c_str(), -1, SQLITE_STATIC);
        },
        [&cards, this](sqlite3_stmt* stmt) -> bool {
            Kanban::Card card;
            card.id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            card.title = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            card.description = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
            card.priority = static_cast<Kanban::Priority>(sqlite3_column_int(stmt, 3));
            card.status = static_cast<Kanban::CardStatus>(sqlite3_column_int(stmt, 4));
            card.color.r = sqlite3_column_double(stmt, 5);
            card.color.g = sqlite3_column_double(stmt, 6);
            card.color.b = sqlite3_column_double(stmt, 7);
            card.color.a = sqlite3_column_double(stmt, 8);
            card.assignee = sqlite3_column_text(stmt, 9) ? (char*)sqlite3_column_text(stmt, 9) : "";
            card.dueDate = sqlite3_column_text(stmt, 10) ? (char*)sqlite3_column_text(stmt, 10) : "";
            card.createdAt = StringToTimePoint(sqlite3_column_text(stmt, 11) ? (char*)sqlite3_column_text(stmt, 11) : "");
            card.modifiedAt = StringToTimePoint(sqlite3_column_text(stmt, 12) ? (char*)sqlite3_column_text(stmt, 12) : "");
            
            LoadTagsForCard(card);
            cards.push_back(std::make_shared<Kanban::Card>(std::move(card)));
            return true;
        });

    return cards;
}

bool KanbanDatabase::UpdateCard(const Kanban::Card& card)
{
    const std::string sql = R"(
        UPDATE kanban_cards 
        SET title = ?, description = ?, priority = ?, status = ?,
            color_r = ?, color_g = ?, color_b = ?, color_a = ?,
            assignee = ?, due_date = ?, modified_at = ?
        WHERE id = ?
    )";

    return m_dbManager->ExecuteSQL(sql, [&card, this](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, card.title.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, card.description.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, static_cast<int>(card.priority));
        sqlite3_bind_int(stmt, 4, static_cast<int>(card.status));
        sqlite3_bind_double(stmt, 5, card.color.r);
        sqlite3_bind_double(stmt, 6, card.color.g);
        sqlite3_bind_double(stmt, 7, card.color.b);
        sqlite3_bind_double(stmt, 8, card.color.a);
        sqlite3_bind_text(stmt, 9, card.assignee.c_str(), -1, SQLITE_STATIC);
        
        if (!card.dueDate.empty()) {
            sqlite3_bind_text(stmt, 10, card.dueDate.c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 10);
        }
        
        sqlite3_bind_text(stmt, 11, TimePointToString(std::chrono::system_clock::now()).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 12, card.id.c_str(), -1, SQLITE_STATIC);
    });
}

bool KanbanDatabase::DeleteCard(const std::string& cardId)
{
    const std::string sql = "DELETE FROM kanban_cards WHERE id = ?";
    
    return m_dbManager->ExecuteSQL(sql, [&cardId](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, cardId.c_str(), -1, SQLITE_STATIC);
    });
}

bool KanbanDatabase::UpdateCardOrder(const std::string& columnId, const std::vector<std::string>& cardIds)
{
    if (!m_dbManager->BeginTransaction()) {
        return false;
    }

    bool success = true;
    for (size_t i = 0; i < cardIds.size(); ++i) {
        const std::string sql = "UPDATE kanban_cards SET card_order = ? WHERE id = ? AND column_id = ?";
        
        if (!m_dbManager->ExecuteSQL(sql, [&cardIds, i, &columnId](sqlite3_stmt* stmt) {
            sqlite3_bind_int(stmt, 1, static_cast<int>(i));
            sqlite3_bind_text(stmt, 2, cardIds[i].c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, columnId.c_str(), -1, SQLITE_STATIC);
        })) {
            success = false;
            break;
        }
    }

    if (success) {
        m_dbManager->CommitTransaction();
    } else {
        m_dbManager->RollbackTransaction();
    }

    return success;
}

bool KanbanDatabase::MoveCard(const std::string& cardId, const std::string& targetColumnId, int targetIndex)
{
    if (!m_dbManager->BeginTransaction()) {
        return false;
    }

    // If targetIndex is -1, get the next available order
    int order = targetIndex;
    if (order == -1) {
        order = GetNextOrder(targetColumnId, "card_order", "column_id");
    }

    // Update the card's column and position
    const std::string sql = R"(
        UPDATE kanban_cards 
        SET column_id = ?, card_order = ?, modified_at = ? 
        WHERE id = ?
    )";

    bool success = m_dbManager->ExecuteSQL(sql, [&cardId, &targetColumnId, order, this](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, targetColumnId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, order);
        sqlite3_bind_text(stmt, 3, TimePointToString(std::chrono::system_clock::now()).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, cardId.c_str(), -1, SQLITE_STATIC);
    });

    if (success) {
        m_dbManager->CommitTransaction();
    } else {
        m_dbManager->RollbackTransaction();
    }

    return success;
}

bool KanbanDatabase::IsCardExists(const std::string& cardId)
{
    bool exists = false;
    
    const std::string sql = "SELECT 1 FROM kanban_cards WHERE id = ? LIMIT 1";
    
    m_dbManager->ExecuteQuery(sql,
        [&cardId](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, cardId.c_str(), -1, SQLITE_STATIC);
        },
        [&exists](sqlite3_stmt* stmt) -> bool {
            exists = true;
            return false; // Stop after first result
        });

    return exists;
}

bool KanbanDatabase::LoadBoardsForProject(Kanban::Project& project)
{
    std::vector<std::unique_ptr<Kanban::Board>> boards;
    
    const std::string sql = R"(
        SELECT id, name, description, created_at, modified_at
        FROM kanban_boards WHERE project_id = ? ORDER BY created_at DESC
    )";

    m_dbManager->ExecuteQuery(sql,
        [&project](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, project.id.c_str(), -1, SQLITE_STATIC);
        },
        [&boards, this](sqlite3_stmt* stmt) -> bool {
            auto board = std::make_unique<Kanban::Board>();
            board->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            board->name = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            board->description = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
            board->createdAt = StringToTimePoint(sqlite3_column_text(stmt, 3) ? (char*)sqlite3_column_text(stmt, 3) : "");
            board->modifiedAt = StringToTimePoint(sqlite3_column_text(stmt, 4) ? (char*)sqlite3_column_text(stmt, 4) : "");
            
            if (LoadColumnsForBoard(*board)) {
                boards.push_back(std::move(board));
            }
            return true;
        });

    project.boards = std::move(boards);
    return !project.boards.empty();
}

bool KanbanDatabase::LoadColumnsForBoard(Kanban::Board& board)
{
    std::vector<std::unique_ptr<Kanban::Column>> columns;
    
    const std::string sql = R"(
        SELECT id, name, header_color_r, header_color_g, header_color_b, header_color_a,
               card_limit, is_collapsed, column_order
        FROM kanban_columns WHERE board_id = ? ORDER BY column_order
    )";

    m_dbManager->ExecuteQuery(sql,
        [&board](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, board.id.c_str(), -1, SQLITE_STATIC);
        },
        [&columns, this](sqlite3_stmt* stmt) -> bool {
            auto column = std::make_unique<Kanban::Column>();
            column->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            column->name = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            column->headerColor.r = sqlite3_column_double(stmt, 2);
            column->headerColor.g = sqlite3_column_double(stmt, 3);
            column->headerColor.b = sqlite3_column_double(stmt, 4);
            column->headerColor.a = sqlite3_column_double(stmt, 5);
            column->cardLimit = sqlite3_column_int(stmt, 6);
            column->isCollapsed = sqlite3_column_int(stmt, 7) != 0;

            if (LoadCardsForColumn(*column)) {
                columns.push_back(std::move(column));
            }
            return true;
        });
    // Logger::Debug("TEST 29. Loaded {} columns for board {}", columns.size(), board.id);
    board.columns = std::move(columns);
    return !board.columns.empty();
}

bool KanbanDatabase::LoadCardsForColumn(Kanban::Column& column)
{
    std::vector<std::shared_ptr<Kanban::Card>> cards;
    
    const std::string sql = R"(
        SELECT id, title, description, priority, status, color_r, color_g, color_b, color_a,
               assignee, due_date, created_at, modified_at
        FROM kanban_cards WHERE column_id = ? ORDER BY card_order
    )";

    m_dbManager->ExecuteQuery(sql,
        [&column](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, column.id.c_str(), -1, SQLITE_STATIC);
        },
        [&cards, this](sqlite3_stmt* stmt) -> bool {
            auto card = std::make_shared<Kanban::Card>();
            card->id = sqlite3_column_text(stmt, 0) ? (char*)sqlite3_column_text(stmt, 0) : "";
            card->title = sqlite3_column_text(stmt, 1) ? (char*)sqlite3_column_text(stmt, 1) : "";
            card->description = sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2) : "";
            card->priority = static_cast<Kanban::Priority>(sqlite3_column_int(stmt, 3));
            card->status = static_cast<Kanban::CardStatus>(sqlite3_column_int(stmt, 4));
            card->color.r = sqlite3_column_double(stmt, 5);
            card->color.g = sqlite3_column_double(stmt, 6);
            card->color.b = sqlite3_column_double(stmt, 7);
            card->color.a = sqlite3_column_double(stmt, 8);
            card->assignee = sqlite3_column_text(stmt, 9) ? (char*)sqlite3_column_text(stmt, 9) : "";
            card->dueDate = sqlite3_column_text(stmt, 10) ? (char*)sqlite3_column_text(stmt, 10) : "";
            card->createdAt = StringToTimePoint(sqlite3_column_text(stmt, 11) ? (char*)sqlite3_column_text(stmt, 11) : "");
            card->modifiedAt = StringToTimePoint(sqlite3_column_text(stmt, 12) ? (char*)sqlite3_column_text(stmt, 12) : "");

            LoadTagsForCard(*card);
            cards.push_back(std::move(card));
            return true;
        });

    column.cards = std::move(cards);
    return true;
}

bool KanbanDatabase::LoadTagsForCard(Kanban::Card& card)
{
    std::vector<std::string> tags;
    
    const std::string sql = R"(
        SELECT t.name 
        FROM kanban_tags t
        JOIN kanban_card_tags ct ON t.id = ct.tag_id
        WHERE ct.card_id = ?
        ORDER BY t.name
    )";

    m_dbManager->ExecuteQuery(sql,
        [&card](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, card.id.c_str(), -1, SQLITE_STATIC);
        },
        [&tags](sqlite3_stmt* stmt) -> bool {
            const char* tagName = (char*)sqlite3_column_text(stmt, 0);
            if (tagName) {
                tags.emplace_back(tagName);
            }
            return true;
        });

    card.tags = std::move(tags);
    return !card.tags.empty();
}

