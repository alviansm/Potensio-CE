#pragma once

#include "DatabaseManager.h"
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <sstream>
#include <iomanip>

// Activity Monitoring Database
class AMDatabase
{
public:
    AMDatabase(std::shared_ptr<DatabaseManager> dbManager);
    ~AMDatabase() = default;

    // Database initialization
    bool Initialize();

    // Utility methods
    std::string GetLastError() const { return m_lastError; }

private:
    std::shared_ptr<DatabaseManager> m_dbManager;
    std::string m_lastError;

    // Schema versioning
    static constexpr int CURRENT_SCHEMA_VERSION = 1;
};