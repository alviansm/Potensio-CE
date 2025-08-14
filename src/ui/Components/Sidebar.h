#pragma once

#include "ui/Windows/MainWindow.h" // For ModulePage enum
#include "imgui.h"

struct SidebarItem
{
    ModulePage module;
    const char* name;
    const char* icon;
    bool enabled;
    ImTextureID iconTexture;
};

class Sidebar
{
public:
    Sidebar();
    ~Sidebar();

    // Update and render
    void Update(float deltaTime);
    void Render();
    
    // Navigation
    ModulePage GetSelectedModule() const { return m_selectedModule; }
    void SetSelectedModule(ModulePage module);
    
    // Appearance
    float GetWidth() const { return m_width; }
    void SetWidth(float width) { m_width = width; }

    // Resources
    void InitializeResources();

private:
    ModulePage m_selectedModule = ModulePage::Dashboard;
    float m_width = 90.0f;
    
    // Sidebar items
    static const SidebarItem s_menuItems[];
    static const size_t s_menuItemCount;
    
    // Rendering helpers
    void RenderMenuItem(const SidebarItem& item, bool isSelected);
    void RenderLogo();
    void RenderFooter();
    
    // Animation and interaction
    float m_animationTime = 0.0f;
    int m_hoveredItem = -1;

    // Sidebar item
    SidebarItem m_menuItems[6];  // fixed size for now, change to std::vector<SidebarItem> vector later if required for plugins
    size_t m_menuItemCount = 6;

    // Page
    bool m_isSelected = false;

    // Icon image
    ImTextureID iconDashboard   = nullptr;
    ImTextureID iconTimer   = nullptr;
    ImTextureID iconKanban   = nullptr;
    ImTextureID iconTodo   = nullptr;
    ImTextureID iconClipboard   = nullptr;
    ImTextureID iconConvert   = nullptr;
    ImTextureID iconGear   = nullptr;
};