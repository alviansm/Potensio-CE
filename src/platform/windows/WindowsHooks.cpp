// src/platform/windows/WindowsHooks.cpp
#include "WindowsHooks.h"
#include "core/Logger.h"

std::map<int, WindowsHooks::HotkeyCallback> WindowsHooks::s_hotkeyCallbacks;
bool WindowsHooks::s_initialized = false;
HHOOK WindowsHooks::s_keyboardHook = nullptr;

bool WindowsHooks::Initialize()
{
    if (s_initialized)
        return true;
        
    s_initialized = true;
    Logger::Info("Windows hooks initialized");
    return true;
}

void WindowsHooks::Cleanup()
{
    if (!s_initialized)
        return;
        
    // Unregister all hotkeys
    for (const auto& pair : s_hotkeyCallbacks)
    {
        UnregisterHotKey(nullptr, pair.first);
    }
    s_hotkeyCallbacks.clear();
    
    // Remove keyboard hook if set
    if (s_keyboardHook)
    {
        UnhookWindowsHookEx(s_keyboardHook);
        s_keyboardHook = nullptr;
    }
    
    s_initialized = false;
    Logger::Info("Windows hooks cleaned up");
}

bool WindowsHooks::RegisterHotkeyCallback(int id, UINT modifiers, UINT virtualKey, HotkeyCallback callback)
{
    if (!s_initialized)
        return false;
        
    // Register system-wide hotkey
    if (!RegisterHotKey(nullptr, id, modifiers, virtualKey))
    {
        DWORD error = GetLastError();
        Logger::Error("Failed to register hotkey {}: Error {}", id, error);
        return false;
    }
    
    // Store callback
    s_hotkeyCallbacks[id] = callback;
    
    Logger::Debug("Registered hotkey {} with modifiers {} and key {}", id, modifiers, virtualKey);
    return true;
}

bool WindowsHooks::UnregisterHotkey(int id)
{
    if (!s_initialized)
        return false;
        
    // Unregister system hotkey
    if (!UnregisterHotKey(nullptr, id))
    {
        Logger::Warning("Failed to unregister hotkey {}", id);
        return false;
    }
    
    // Remove callback
    s_hotkeyCallbacks.erase(id);
    
    Logger::Debug("Unregistered hotkey {}", id);
    return true;
}

bool WindowsHooks::ProcessMessage(MSG* msg)
{
    if (!s_initialized || !msg)
        return false;
        
    // Handle hotkey messages
    if (msg->message == WM_HOTKEY)
    {
        int hotkeyId = static_cast<int>(msg->wParam);
        
        auto it = s_hotkeyCallbacks.find(hotkeyId);
        if (it != s_hotkeyCallbacks.end())
        {
            Logger::Debug("Hotkey {} triggered", hotkeyId);
            it->second(); // Call the callback
            return true;
        }
    }
    
    return false;
}

LRESULT CALLBACK WindowsHooks::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0)
    {
        // Handle keyboard events here if needed
        // For now, just pass through
    }
    
    return CallNextHookEx(s_keyboardHook, nCode, wParam, lParam);
}