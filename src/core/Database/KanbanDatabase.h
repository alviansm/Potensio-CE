#pragma once

#include "DatabaseManager.h"
#include "core/Kanban/KanbanManager.h"
#include <memory>
#include <vector>
#include <chrono>

class KanbanManager;

class KanbanDatabase {
public:
  KanbanDatabase(std::shared_ptr<DatabaseManager> dbManager);
  ~KanbanDatabase() = default;

  // Database initialization
  bool Initialize();
  bool CreateTables();
  bool MigrateSchema(int fromVersion, int toVersion);

public:
  // Queries
  //std::vector<std::unique_ptr<Kanban::Project>> ReadAllProjects();


private:
  std::shared_ptr<DatabaseManager> m_dbManager;

  // Schema versions
  static constexpr int CURRENT_SCHEMA_VERSION = 1;

  // Table creation methods
  bool CreateProjectsTable();
  bool CreateBoardsTable();
  bool CreateColumnsTable();
  bool CreateCardsTable();
  bool CreateTagsTable();
  bool CreateCardsTagsNormalizationTable();
  bool CreateKanbanSettingTable();

  bool CreateIndexes(); // For performance

  // Migration methods
  bool MigrateToVersion1();
};