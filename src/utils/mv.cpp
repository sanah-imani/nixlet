#include "util.h"
#include "../fs/vfs.h"

namespace Utils {

std::string mv(Shell& sh, const Command& cmd) {
    if (cmd.argv.size() < 3) {
        sh.set_exit_code(1);
        return "mv: missing operand\n";
    }

    const std::string& dst = cmd.argv.back();
    for (size_t i = 1; i < cmd.argv.size() - 1; ++i) {
        if (!VFS::get().rename(cmd.argv[i], dst, sh.cwd())) {
            sh.set_exit_code(1);
            return "mv: " + cmd.argv[i] + ": " + VFS::get().last_error() + "\n";
        }
    }
    sh.set_exit_code(0);
    return "";
}

} // namespace Utils
