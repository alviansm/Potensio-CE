#include "KanbanDatabase.h"
#include "core/Logger.h"
#include "sqlite3.h"
#include <sstream>
#include <iomanip>
#include <ctime>

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

//std::vector<std::unique_ptr<Kanban::Project>>
//KanbanDatabase::ReadAllProjects() 
//{
//  return std::vector<std::unique_ptr<Kanban::Project>>();
//}

bool KanbanDatabase::MigrateToVersion1() { return false; }

bool KanbanDatabase::CreateProjectsTable() 
{ 
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS projects (
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
        CREATE TABLE IF NOT EXISTS boards (
            id TEXT PRIMARY KEY,
            project_id TEXT NOT NULL,
            name TEXT NOT NULL,
            description TEXT,
            is_active INTEGER DEFAULT 1,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            modified_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
        );
  )";

  return m_dbManager->ExecuteSQL(sql);
}

bool KanbanDatabase::CreateColumnsTable() 
{ 
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS columns (
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
            FOREIGN KEY (board_id) REFERENCES boards(id) ON DELETE CASCADE
        );
  )";

  return m_dbManager->ExecuteSQL(sql);
}

bool KanbanDatabase::CreateCardsTable()
{ 
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS cards (
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
            FOREIGN KEY (column_id) REFERENCES columns(id) ON DELETE CASCADE
        );
  )";

  return m_dbManager->ExecuteSQL(sql);
}

bool KanbanDatabase::CreateTagsTable() 
{ 
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS tags (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL
        );
  )";

  return m_dbManager->ExecuteSQL(sql);
}

bool KanbanDatabase::CreateCardsTagsNormalizationTable() 
{ 
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS card_tags (
            card_id TEXT,
            tag_id INTEGER,
            PRIMARY KEY (card_id, tag_id),
            FOREIGN KEY (card_id) REFERENCES cards(id) ON DELETE CASCADE,
            FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
        );
  )";

  return m_dbManager->ExecuteSQL(sql);
}

bool KanbanDatabase::CreateKanbanSettingTable() 
{ 
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS settings (
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
        CREATE INDEX IF NOT EXISTS idx_boards_project_id ON boards(project_id);
        CREATE INDEX IF NOT EXISTS idx_columns_board_id ON columns(board_id);
        CREATE INDEX IF NOT EXISTS idx_cards_column_id ON cards(column_id);
        CREATE INDEX IF NOT EXISTS idx_cards_status ON cards(status);
        CREATE INDEX IF NOT EXISTS idx_cards_priority ON cards(priority);
        CREATE INDEX IF NOT EXISTS idx_cards_due_date ON cards(due_date);
  )";

  return m_dbManager->ExecuteSQL(sql);
}
