# Emscripten CMake toolchain file
set(CMAKE_SYSTEM_NAME Emscripten)

# Let Emscripten's own toolchain file handle the rest
# This file is used via: cmake -DCMAKE_TOOLCHAIN_FILE=toolchain-wasm.cmake
# Emscripten sets up CC/CXX automatically when invoked via emcmake
