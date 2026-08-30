#include "util.h"
#include "../fs/vfs.h"

namespace Utils {

std::string touch(Shell& sh, const Command& cmd) {
    if (cmd.argv.size() < 2) {
        sh.set_exit_code(1);
        return "touch: missing operand\n";
    }
    for (size_t i = 1; i < cmd.argv.size(); ++i) {
        if (!VFS::get().touch(cmd.argv[i], sh.cwd())) {
            sh.set_exit_code(1);
            return "touch: " + cmd.argv[i] + ": " + VFS::get().last_error() + "\n";
        }
    }
    sh.set_exit_code(0);
    return "";
}

} // namespace Utils
