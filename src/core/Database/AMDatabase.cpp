#include "AMDatabase.h"
#include "core/Logger.h"
#include "sqlite3.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>

AMDatabase::AMDatabase(std::shared_ptr<DatabaseManager> dbManager)
    : m_dbManager(dbManager) {}

bool AMDatabase::Initialize() {
  if (!m_dbManager || !m_dbManager->IsConnected()) {
    Logger::Error("AMDatabase: Database manager not available");
    return false;
  }

  // Create tables if they don't exist
//   if (!CreateTables()) {
//     Logger::Error("KanbanDatabase: Failed to create tables");
//     return false;
//   }

//   // Handle schema migrations
//   int currentVersion = m_dbManager->GetSchemaVersion();
//   if (currentVersion < CURRENT_SCHEMA_VERSION) {
//     Logger::Info("KanbanDatabase: Migrating schema from version {} to {}",
//                  currentVersion, CURRENT_SCHEMA_VERSION);

//     if (!MigrateSchema(currentVersion, CURRENT_SCHEMA_VERSION)) {
//       Logger::Error("KanbanDatabase: Schema migration failed");
//       return false;
//     }

//     m_dbManager->SetSchemaVersion(CURRENT_SCHEMA_VERSION);
//   }

//   Logger::Info("KanbanDatabase initialized successfully");
  return true;
}