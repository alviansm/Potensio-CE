// src/app/AppConfig.h
#pragma once

#include <string>
#include <map>
#include <variant>

class AppConfig
{
public:
    using ConfigValue = std::variant<bool, int, float, std::string>;

    AppConfig();
    ~AppConfig();

    // Load configuration from file
    bool Load();
    
    // Save configuration to file
    bool Save();
    
    // Get configuration values with defaults
    bool GetValue(const std::string& key, bool defaultValue) const;
    int GetValue(const std::string& key, int defaultValue) const;
    float GetValue(const std::string& key, float defaultValue) const;
    std::string GetValue(const std::string& key, const std::string& defaultValue) const;
    
    // Set configuration values
    void SetValue(const std::string& key, bool value);
    void SetValue(const std::string& key, int value);
    void SetValue(const std::string& key, float value);
    void SetValue(const std::string& key, const std::string& value);
    
    // Check if key exists
    bool HasKey(const std::string& key) const;
    
    // Remove key
    void RemoveKey(const std::string& key);
    
    // Clear all settings
    void Clear();

private:
    std::map<std::string, ConfigValue> m_config;
    std::string m_configFile;
    bool m_isDirty = false;
    
    // Helper methods
    void SetDefaults();
    std::string GetConfigFilePath() const;
    
    // JSON-like parsing (simple implementation)
    bool ParseConfigLine(const std::string& line);
    std::string SerializeValue(const ConfigValue& value) const;
};