#include "util.h"
#include "../fs/vfs.h"

namespace Utils {

std::string rm(Shell& sh, const Command& cmd) {
    bool recursive = false;
    bool force     = false;
    std::vector<std::string> targets;

    for (size_t i = 1; i < cmd.argv.size(); ++i) {
        const auto& arg = cmd.argv[i];
        if (!arg.empty() && arg[0] == '-') {
            for (size_t j = 1; j < arg.size(); ++j) {
                if (arg[j] == 'r' || arg[j] == 'R') recursive = true;
                if (arg[j] == 'f') force = true;
            }
        } else {
            targets.push_back(arg);
        }
    }

    if (targets.empty()) {
        sh.set_exit_code(1);
        return "rm: missing operand\n";
    }

    for (const auto& target : targets) {
        auto entry = VFS::get().lstat(target, sh.cwd());
        if (!entry) {
            if (force) continue;
            sh.set_exit_code(1);
            return "rm: " + target + ": " + VFS::get().last_error() + "\n";
        }

        bool ok = (entry->type == InodeType::Directory)
            ? (recursive ? VFS::get().rmdir(target, sh.cwd(), true)
                         : (sh.set_exit_code(1), false))
            : VFS::get().unlink(target, sh.cwd());

        if (!ok && !force) {
            sh.set_exit_code(1);
            return "rm: " + target + ": " +
                   (entry->type == InodeType::Directory && !recursive
                       ? "is a directory\n"
                       : VFS::get().last_error() + "\n");
        }
    }

    sh.set_exit_code(0);
    return "";
}

} // namespace Utils
