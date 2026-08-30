#include "util.h"
#include "../fs/vfs.h"

namespace Utils {

std::string cp(Shell& sh, const Command& cmd) {
    bool recursive = false;
    std::vector<std::string> operands;

    for (size_t i = 1; i < cmd.argv.size(); ++i) {
        const auto& arg = cmd.argv[i];
        if (!arg.empty() && arg[0] == '-') {
            for (size_t j = 1; j < arg.size(); ++j)
                if (arg[j] == 'r' || arg[j] == 'R') recursive = true;
        } else {
            operands.push_back(arg);
        }
    }

    if (operands.size() < 2) {
        sh.set_exit_code(1);
        return "cp: missing destination\n";
    }

    const std::string& dst = operands.back();
    for (size_t i = 0; i < operands.size() - 1; ++i) {
        if (!VFS::get().copy(operands[i], dst, sh.cwd(), recursive)) {
            sh.set_exit_code(1);
            return "cp: " + operands[i] + ": " + VFS::get().last_error() + "\n";
        }
    }
    sh.set_exit_code(0);
    return "";
}

} // namespace Utils
