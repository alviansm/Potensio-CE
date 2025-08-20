cmake_minimum_required(VERSION 3.16)
project(Potensio VERSION 0.1.0 LANGUAGES CXX C)  # Added C language

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find packages
find_package(OpenGL REQUIRED)

# SQLite setup - FIXED VERSION
set(SQLITE_DIR ${CMAKE_SOURCE_DIR}/external/sqlite)

# Verify SQLite files exist
if(NOT EXISTS "${SQLITE_DIR}/sqlite3.c")
    message(FATAL_ERROR "SQLite source file not found at: ${SQLITE_DIR}/sqlite3.c")
endif()

if(NOT EXISTS "${SQLITE_DIR}/sqlite3.h")
    message(FATAL_ERROR "SQLite header file not found at: ${SQLITE_DIR}/sqlite3.h")
endif()

message(STATUS "Found SQLite at: ${SQLITE_DIR}")

# Create a separate SQLite library
add_library(sqlite3 STATIC ${SQLITE_DIR}/sqlite3.c "src/ui/FileDropTarget.h" "src/app/MouseTracker.h" "src/core/Notify.h" "src/core/Notify.cpp")

# Set SQLite to compile as C (not C++)
set_target_properties(sqlite3 PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

# SQLite compile definitions
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_FTS3
    SQLITE_ENABLE_FTS3_PARENTHESIS
    SQLITE_ENABLE_RTREE
    SQLITE_ENABLE_JSON1
    SQLITE_OMIT_LOAD_EXTENSION
)

# SQLite include directory
target_include_directories(sqlite3 PUBLIC ${SQLITE_DIR})

# Disable warnings for SQLite (it's external code)
if(MSVC)
    target_compile_options(sqlite3 PRIVATE 
        /w  # Disable all warnings for SQLite
    )
else()
    target_compile_options(sqlite3 PRIVATE 
        -w  # Disable all warnings for SQLite
    )
endif()

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
    src/core/Kanban/KanbanManager.cpp
    src/core/Todo/TodoManager.cpp
    src/core/Clipboard/ClipboardManager.cpp
    src/core/Database/DatabaseManager.cpp
    src/core/Database/PomodoroDatabase.cpp
    src/ui/UIManager.cpp
    src/ui/Components/Sidebar.cpp
    src/ui/Windows/MainWindow.cpp
    src/ui/Windows/SettingsWindow.cpp
    src/ui/Windows/PomodoroWindow.cpp
    src/ui/Windows/KanbanWindow.cpp
    src/ui/Windows/ClipboardWindow.cpp
    src/platform/windows/WindowsUtils.cpp
    src/platform/windows/WindowsHooks.cpp
    src/core/FileConverter/FileConverter.cpp
    resources/app.rc
)

# Verify that Database source files exist
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/src/core/Database/DatabaseManager.cpp")
    message(FATAL_ERROR "DatabaseManager.cpp not found at: ${CMAKE_SOURCE_DIR}/src/core/Database/DatabaseManager.cpp")
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/src/core/Database/PomodoroDatabase.cpp")
    message(FATAL_ERROR "PomodoroDatabase.cpp not found at: ${CMAKE_SOURCE_DIR}/src/core/Database/PomodoroDatabase.cpp")
endif()

# Add resource file on Windows (optional)
if(WIN32)
    message(STATUS "Windows resource file disabled - using default icon")
endif()

# Create executable with explicit source grouping
add_executable(Potensio 
    ${APP_SOURCES}
    ${IMGUI_SOURCES}
    # SQLite is now a separate library, not included here
 "src/ui/FileDropTarget.h" "src/app/MouseTracker.h" "src/core/Notify.h" "src/core/Notify.cpp")

# Include directories
target_include_directories(Potensio PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_SOURCE_DIR}/external/stb
    ${SQLITE_DIR}  # Keep this for headers
    resources
)

# Link libraries - IMPORTANT: Added sqlite3 library here
target_link_libraries(Potensio PRIVATE
    sqlite3  # This is the key addition
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
    runtimeobject.lib
)

# Compiler flags with /FS to fix PDB issues
if(MSVC)
    target_compile_options(Potensio PRIVATE /W4 /FS)
    # Disable specific warnings for ImGui and other external code in main target
    target_compile_options(Potensio PRIVATE 
        /wd4996  # Disable deprecated function warnings
        /wd4244  # Disable conversion warnings
        /wd4267  # Disable size_t conversion warnings
    )
    
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

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(Potensio PRIVATE stdc++fs)
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

source_group("Source Files\\Core\\Kanban" FILES 
    src/core/Kanban/KanbanManager.cpp
)

source_group("Source Files\\Core\\Todo" FILES 
    src/core/Todo/TodoManager.cpp
)

source_group("Source Files\\Core\\Clipboard" FILES 
    src/core/Clipboard/ClipboardManager.cpp
)

source_group("Source Files\\Core\\Database" FILES 
    src/core/Database/DatabaseManager.cpp
    src/core/Database/PomodoroDatabase.cpp
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
    src/ui/Windows/KanbanWindow.cpp
    src/ui/Windows/ClipboardWindow.cpp
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

source_group("Header Files\\Core\\Kanban" FILES 
    src/core/Kanban/KanbanManager.h
)

source_group("Header Files\\Core\\Todo" FILES 
    src/core/Todo/TodoManager.h
)

source_group("Header Files\\Core\\Clipboard" FILES 
    src/core/Clipboard/ClipboardManager.h
)

source_group("Header Files\\Core\\Database" FILES 
    src/core/Database/DatabaseManager.h
    src/core/Database/PomodoroDatabase.h
)

source_group("Header Files\\UI\\Windows" FILES 
    src/ui/Windows/PomodoroWindow.h
    src/ui/Windows/KanbanWindow.h
    src/ui/Windows/ClipboardWindow.h
)

source_group("Source Files\\Core\\FileConverter" FILES 
    src/core/FileConverter/FileConverter.cpp
)

source_group("Header Files\\Core\\FileConverter" FILES 
    src/core/FileConverter/FileConverter.h
)

source_group("External\\SQLite" FILES 
    ${SQLITE_DIR}/sqlite3.c
    ${SQLITE_DIR}/sqlite3.h
)

source_group("External\\ImGui" FILES 
    ${IMGUI_SOURCES}
)

# Print configuration summary
message(STATUS "=== Potensio Build Configuration ===")
message(STATUS "SQLite source: ${SQLITE_DIR}/sqlite3.c")
message(STATUS "SQLite library: sqlite3 (static)")
message(STATUS "Database sources: DatabaseManager.cpp, PomodoroDatabase.cpp")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "=====================================")