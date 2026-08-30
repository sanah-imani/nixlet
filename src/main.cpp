#include <emscripten/emscripten.h>
#include <string>
#include <cstring>
#include <cstdlib>

// Forward declarations (implementations will live in shell/, fs/, proc/, utils/)
// For now: stub that echoes back, so we can verify the WASM pipeline.

static std::string g_output_buf;

extern "C" {

EMSCRIPTEN_KEEPALIVE
void nixlet_init() {
    // TODO: initialise VFS, shell state, process table
    g_output_buf.clear();
}

// Takes a null-terminated command line string.
// Returns a pointer to a null-terminated output string (valid until next call).
EMSCRIPTEN_KEEPALIVE
const char* nixlet_input(const char* line) {
    if (!line) return "";

    // Stub: echo the command back until real shell is wired in.
    g_output_buf = std::string("nixlet: ") + line + " (shell not yet implemented)\n";
    return g_output_buf.c_str();
}

EMSCRIPTEN_KEEPALIVE
void nixlet_free() {
    g_output_buf.clear();
    g_output_buf.shrink_to_fit();
}

} // extern "C"
