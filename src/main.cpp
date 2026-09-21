#include <emscripten/emscripten.h>
#include <string>
#include "shell/shell.h"
#include "fs/vfs.h"

static Shell       g_shell;
static std::string g_output_buf;

extern "C" {

EMSCRIPTEN_KEEPALIVE
void nixlet_init() {
    g_shell.init();
}

EMSCRIPTEN_KEEPALIVE
const char* nixlet_input(const char* line) {
    g_output_buf = g_shell.execute(line ? line : "");
    return g_output_buf.c_str();
}

EMSCRIPTEN_KEEPALIVE
void nixlet_free() {
    VFS::get().reset();
    g_output_buf.clear();
    g_output_buf.shrink_to_fit();
}

EMSCRIPTEN_KEEPALIVE
const char* nixlet_serialize(){
    g_output_buf = VFS::get().serialize();
    return g_output_buf.c_str();
}

EMSCRIPTEN_KEEPALIVE
bool nixlet_deserialize(const char* json){
    if (!json) return false;
    return VFS::get().deserialize(json);
}

EMSCRIPTEN_KEEPALIVE
const char* nixlet_cwd() {
    g_output_buf = g_shell.cwd();
    return g_output_buf.c_str();
}
} // extern "C"
