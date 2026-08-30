cmake_minimum_required(VERSION 3.20)
project(nixlet CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
file(GLOB_RECURSE SOURCES
    src/main.cpp
    src/shell/*.cpp
    src/fs/*.cpp
    src/process/*.cpp
    src/utils/*.cpp
)

add_executable(nixlet ${SOURCES})

target_include_directories(nixlet PRIVATE src)

# Emscripten-specific flags
if(EMSCRIPTEN)
    set_target_properties(nixlet PROPERTIES
        OUTPUT_NAME "nixlet"
        SUFFIX ".js"
    )
    target_link_options(nixlet PRIVATE
        -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8
        -sEXPORTED_FUNCTIONS=_nixlet_init,_nixlet_input,_nixlet_free
        -sALLOW_MEMORY_GROWTH=1
        -sMODULARIZE=1
        -sEXPORT_NAME=NixletModule
        -sENVIRONMENT=web
        --bind
    )
endif()
