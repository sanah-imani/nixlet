#include "util.h"
#include "../fs/vfs.h"

namespace Utils {

std::string mkdir(Shell& sh, const Command& cmd) {
    bool parents = false;
    std::vector<std::string> dirs;

    for (size_t i = 1; i < cmd.argv.size(); ++i) {
        if (cmd.argv[i] == "-p") parents = true;
        else dirs.push_back(cmd.argv[i]);
    }

    if (dirs.empty()) {
        sh.set_exit_code(1);
        return "mkdir: missing operand\n";
    }
    for (const auto& dir : dirs) {
        if (!VFS::get().mkdir(dir, sh.cwd(), parents)) {
            sh.set_exit_code(1);
            return "mkdir: " + dir + ": " + VFS::get().last_error() + "\n";
        }
    }
    sh.set_exit_code(0);
    return "";
}

} // namespace Utils
