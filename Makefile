EMSDK        := $(HOME)/emsdk
EMSDK_ENV    := source $(EMSDK)/emsdk_env.sh
BUILD_DIR    := build
WEB_DIR      := web

.PHONY: all build serve clean test setup

all: build

## Install npm deps for the web frontend
setup:
	npm install --prefix $(WEB_DIR)

## Compile C++ -> WASM via Emscripten + CMake
build:
	@$(EMSDK_ENV) && \
	  emcmake cmake -S . -B $(BUILD_DIR) && \
	  cmake --build $(BUILD_DIR)
	@echo "Build complete: $(BUILD_DIR)/nixlet.js + nixlet.wasm"

## Serve the web frontend (requires 'build' first)
serve: build
	@echo "Serving at http://localhost:8080"
	npm --prefix $(WEB_DIR) run dev

## Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

## Run C++ unit tests (native build)
test:
	cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Debug
	cmake --build build-native
	ctest --test-dir build-native --output-on-failure
