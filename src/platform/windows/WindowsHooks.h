// src/platform/windows/WindowsHooks.h
#pragma once

#include <windows.h>
#include <functional>
#include <map>

class WindowsHooks
{
public:
    using HotkeyCallback = std::function<void()>;

    // Initialize hooks system
    static bool Initialize();
    
    // Cleanup hooks system
    static void Cleanup();
    
    // Hotkey registration
    static bool RegisterHotkeyCallback(int id, UINT modifiers, UINT virtualKey, HotkeyCallback callback);
    static bool UnregisterHotkey(int id);
    
    // Global message processing
    static bool ProcessMessage(MSG* msg);

private:
    static std::map<int, HotkeyCallback> s_hotkeyCallbacks;
    static bool s_initialized;
    
    // Message handling
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    
    // Internal state
    static HHOOK s_keyboardHook;
};