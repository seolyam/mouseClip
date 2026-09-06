cmake_minimum_required(VERSION 3.20)
project(mouseClip LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    src/main.cpp
    src/clip_manager.cpp
    src/clip_manager.hpp
    src/target_detector.cpp
    src/target_detector.hpp
    src/tray_manager.cpp
    src/tray_manager.hpp
    src/resource.rc
)

# 1. Release Windowed Executable (No console window, runs in tray/background)
add_executable(mouseClip WIN32 ${SOURCES})
target_link_libraries(mouseClip PRIVATE user32 kernel32 shell32 gdi32)

# 2. Debug Console Executable (Console attached, live diagnostics)
add_executable(mouseClip_debug ${SOURCES})
target_compile_definitions(mouseClip_debug PRIVATE _CONSOLE)
target_link_libraries(mouseClip_debug PRIVATE user32 kernel32 shell32 gdi32)

# Use static runtime on MSVC so binary is standalone with zero runtime DLL dependencies
if(MSVC)
    set_property(TARGET mouseClip PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set_property(TARGET mouseClip_debug PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set_property(TARGET mouseClip APPEND_STRING PROPERTY LINK_FLAGS " /MANIFEST:NO")
    set_property(TARGET mouseClip_debug APPEND_STRING PROPERTY LINK_FLAGS " /MANIFEST:NO")
endif()
