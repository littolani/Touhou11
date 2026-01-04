# Enable MASM
enable_language(ASM_MASM)
set(CHIREIDEN_DEF "src/d3d9.def")
file(GLOB_RECURSE CHIREIDEN_SOURCES "src/*.cpp")
file(GLOB_RECURSE CHIREIDEN_HEADERS "include/*.h")
set(CHIREIDEN_ASM "src/Symbols.asm")

add_library(Chireiden SHARED 
    ${CHIREIDEN_SOURCES} 
    ${CHIREIDEN_HEADERS} 
    ${CHIREIDEN_ASM}
    ${CHIREIDEN_DEF}
)

set_target_properties(Chireiden PROPERTIES 
    OUTPUT_NAME "d3d9"
    SUFFIX ".dll"
)

# Include paths
target_include_directories(Chireiden PRIVATE include)

# Dependencies
target_link_libraries(Chireiden PRIVATE imgui DirectX9)

target_link_options(Chireiden PRIVATE "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/d3d9.def")