cmake_minimum_required(VERSION 3.20)
project(Touhou11)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})

# DirectX SDK (June 2010)
if (DEFINED ENV{DXSDK_DIR})
    set(DXSDK_DIR $ENV{DXSDK_DIR})
else()
    set(DXSDK_DIR "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)")
endif()

if (NOT EXISTS ${DXSDK_DIR})
    message(FATAL_ERROR "DirectX SDK not found at ${DXSDK_DIR}. Please install it or set DXSDK_DIR environment variable.")
endif()

add_library(DirectX9 INTERFACE)
target_include_directories(DirectX9 INTERFACE "${DXSDK_DIR}/Include")
target_link_directories(DirectX9 INTERFACE "${DXSDK_DIR}/Lib/x86")
target_link_libraries(DirectX9 INTERFACE d3d9 d3dx9 dinput8 winmm dxguid)

add_subdirectory(third_party/imgui)
add_subdirectory(core)

# Microsoft thinks it knows best and won't run an executable it didn't build.
# It's best if th11.exe runs directly with the local debugger without needing to attach every time.
# Create a dummy source so CMake has something to compile
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp" "int main() { return 0; }")
add_executable(Touhou11 "${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp")
set_target_properties(Touhou11 PROPERTIES OUTPUT_NAME "th11")
target_link_libraries(Touhou11 PRIVATE Chireiden)

# Overwrite the compiled dummy with the REAL game executable
# This runs *after* the linker creates the dummy th11.exe
# Copy All game files, including the real th11.exe
add_custom_command(TARGET Touhou11 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/game_files"
    "$<TARGET_FILE_DIR:Touhou11>"
    COMMENT "Copying game files to output directory..."
)