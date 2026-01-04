# Collect ImGui sources
file(GLOB IMGUI_SOURCES 
    "*.cpp" 
    "backends/imgui_impl_dx9.cpp" 
    "backends/imgui_impl_win32.cpp"
)

add_library(imgui STATIC ${IMGUI_SOURCES})

# Include directories for ImGui itself and its backends
target_include_directories(imgui PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_CURRENT_SOURCE_DIR}/backends
)

# ImGui DX9 backend requires DirectX headers
target_link_libraries(imgui PUBLIC DirectX9)