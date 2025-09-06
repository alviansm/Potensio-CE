#ifndef AMMANAGER_H
#define AMMANAGER_H

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <chrono>

#include "app/AppConfig.h"
#include "core/Database/AMDatabase.h"

class AMManager {
// General methods for Potensio app
public:
    AMManager();
    ~AMManager();

    bool Initialize(AppConfig* config);
    void Shutdown();

    // Config for settings

// Public method related to monitor opened window
public:
    void Start();
    void Stop();
    std::map<std::string, long long> GetReport();

// Private method related to windows-api to monitor opened window
private:
    std::atomic<bool> running;
    std::thread worker;
    std::mutex dataMutex;

    std::map<std::string, long long> usageData; // appName -> ms
    std::string currentApp;
    std::chrono::steady_clock::time_point lastSwitchTime;

    void MonitorLoop();
    std::string GetActiveWindowProcessName();
};

#endif // AMMANAGER_H