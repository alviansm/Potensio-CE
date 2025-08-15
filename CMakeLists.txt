cmake_minimum_required(VERSION 3.16)
project(Potensio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find packages
find_package(OpenGL REQUIRED)

# Dear ImGui setup
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Application sources
set(APP_SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/AppConfig.cpp
    src/app/SystemTray.cpp
    src/core/Logger.cpp
    src/core/Utils.cpp
    src/core/Timer/PomodoroTimer.cpp
    src/ui/UIManager.cpp
    src/ui/Components/Sidebar.cpp
    src/ui/Windows/MainWindow.cpp
    src/ui/Windows/SettingsWindow.cpp
    src/ui/Windows/PomodoroWindow.cpp
    src/platform/windows/WindowsUtils.cpp
    src/platform/windows/WindowsHooks.cpp
    resources/app.rc
)

# Add resource file on Windows (optional)
if(WIN32)
    # Temporarily disabled to avoid RC errors
    # if(EXISTS "${CMAKE_SOURCE_DIR}/resources/app.rc")
    #     list(APPEND APP_SOURCES resources/app.rc)
    #     message(STATUS "Added Windows resource file")
    # else()
        message(STATUS "Windows resource file disabled - using default icon")
    # endif()
endif()

# Create executable
add_executable(Potensio 
    ${APP_SOURCES}
    ${IMGUI_SOURCES}
)

# Include directories
target_include_directories(Potensio PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_SOURCE_DIR}/external/stb
    resources
)

# Link libraries
target_link_libraries(Potensio PRIVATE
    OpenGL::GL
    gdi32
    shell32
    user32
    kernel32
    ole32
    oleaut32
    uuid
    comdlg32
    advapi32
)

# Compiler flags
if(MSVC)
    target_compile_options(Potensio PRIVATE /W4)
    # Enable console for debug builds
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_target_properties(Potensio PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    else()
        set_target_properties(Potensio PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
    endif()
else()
    target_compile_options(Potensio PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Copy resources to build directory
file(COPY ${CMAKE_SOURCE_DIR}/resources DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# Set working directory for Visual Studio
if(MSVC)
    set_target_properties(Potensio PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

# Add custom target for organizing source files in IDE
source_group("Source Files\\App" FILES 
    src/app/Application.cpp
    src/app/AppConfig.cpp
    src/app/SystemTray.cpp
)

source_group("Source Files\\Core" FILES 
    src/core/Logger.cpp
    src/core/Utils.cpp
)

source_group("Source Files\\Core\\Timer" FILES 
    src/core/Timer/PomodoroTimer.cpp
)

source_group("Source Files\\UI" FILES 
    src/ui/UIManager.cpp
)

source_group("Source Files\\UI\\Components" FILES 
    src/ui/Components/Sidebar.cpp
)

source_group("Source Files\\UI\\Windows" FILES 
    src/ui/Windows/MainWindow.cpp
    src/ui/Windows/SettingsWindow.cpp
    src/ui/Windows/PomodoroWindow.cpp
)

source_group("Source Files\\Platform\\Windows" FILES 
    src/platform/windows/WindowsUtils.cpp
    src/platform/windows/WindowsHooks.cpp
)

source_group("Header Files\\App" FILES 
    src/app/Application.h
    src/app/AppConfig.h
    src/app/SystemTray.h
)

source_group("Header Files\\Core\\Timer" FILES 
    src/core/Timer/PomodoroTimer.h
)

source_group("Header Files\\UI\\Windows" FILES 
    src/ui/Windows/PomodoroWindow.h
)