#include "util.h"
#include "../fs/vfs.h"

namespace Utils {

std::string cat(Shell& sh, const Command& cmd) {
    if (cmd.argv.size() < 2) {
        sh.set_exit_code(1);
        return "cat: missing operand\n";
    }
    std::string out;
    for (size_t i = 1; i < cmd.argv.size(); ++i) {
        auto data = VFS::get().read(cmd.argv[i], sh.cwd());
        if (!data) {
            sh.set_exit_code(1);
            return "cat: " + cmd.argv[i] + ": " + VFS::get().last_error() + "\n";
        }
        out += *data;
    }
    sh.set_exit_code(0);
    return out;
}

} // namespace Utils
