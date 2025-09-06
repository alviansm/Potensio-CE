#include "AMManager.h"
#include <windows.h>
#include <psapi.h>

AMManager::AMManager() : running(false) {}

AMManager::~AMManager() { 
    Stop(); 
}

bool AMManager::Initialize(AppConfig* config)
{
    // Add your initialization logic here
    // For now, return true to indicate successful initialization
    return true;
}

void AMManager::Shutdown()
{

}

void AMManager::Start() {
    running = true;
    worker = std::thread(&AMManager::MonitorLoop, this);
}

void AMManager::Stop() {
    running = false;
    if (worker.joinable())
        worker.join();
}

std::map<std::string, long long> AMManager::GetReport() {
    std::lock_guard<std::mutex> lock(dataMutex);
    return usageData;
}

void AMManager::MonitorLoop() {
    lastSwitchTime = std::chrono::steady_clock::now();

    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::string activeApp = GetActiveWindowProcessName();
        if (activeApp.empty()) continue;

        if (activeApp != currentApp) {
            auto now = std::chrono::steady_clock::now();
            long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSwitchTime).count();

            if (!currentApp.empty()) {
                std::lock_guard<std::mutex> lock(dataMutex);
                usageData[currentApp] += duration;
            }

            currentApp = activeApp;
            lastSwitchTime = now;
        }
    }
}

std::string AMManager::GetActiveWindowProcessName() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return "";

    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) return "";

    char exeName[MAX_PATH];
    if (GetModuleFileNameExA(hProcess, NULL, exeName, MAX_PATH)) {
        CloseHandle(hProcess);
        return std::string(exeName);
    }

    CloseHandle(hProcess);
    return "";
}

