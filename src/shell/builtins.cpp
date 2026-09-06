#include "builtins.h"
#include "../fs/vfs.h"
#include "../fs/path.h"
#include <sstream>
#include <algorithm>

namespace Builtins {

std::string cd(Shell& sh, const Command& cmd) {
    std::string target = cmd.argv.size() > 1
        ? cmd.argv[1]
        : sh.get_env("HOME");

    const std::string abs = Path::make_absolute(target, sh.cwd());
    auto result = VFS::get().stat(abs, sh.cwd());

    if (!result) {
        return "cd: " + target + ": " + VFS::get().last_error() + "\n";
    }
    if (result->type != InodeType::Directory) {
        return "cd: " + target + ": Not a directory\n";
    }

    sh.set_cwd(abs);
    sh.set_env("PWD", abs);
    sh.set_exit_code(0);
    return "";
}

std::string pwd(Shell& sh, const Command&) {
    sh.set_exit_code(0);
    return sh.cwd() + "\n";
}

std::string echo(Shell& sh, const Command& cmd) {
    bool newline = true;
    size_t start = 1;

    if (cmd.argv.size() > 1 && cmd.argv[1] == "-n") {
        newline = false;
        start = 2;
    }

    std::string out;
    for (size_t i = start; i < cmd.argv.size(); ++i) {
        if (i > start) out += ' ';
        out += cmd.argv[i];
    }
    if (newline) out += '\n';

    sh.set_exit_code(0);
    return out;
}

std::string exit_(Shell& sh, const Command& cmd) {
    int code = 0;
    if (cmd.argv.size() > 1) {
        try { code = std::stoi(cmd.argv[1]); }
        catch (...) { code = 0; }
    }
    sh.set_exit_code(code);
    return "\x00EXIT";
}

std::string env(Shell& sh, const Command&) {
    std::string out;
    for (auto& [key, val] : sh.env()) {
        out += key + "=" + val + "\n";
    }
    sh.set_exit_code(0);
    return out;
}

std::string export_(Shell& sh, const Command& cmd) {
    if (cmd.argv.size() < 2) {
        sh.set_exit_code(0);
        return "";
    }
    for (size_t i = 1; i < cmd.argv.size(); ++i) {
        const std::string& arg = cmd.argv[i];
        auto pos = arg.find('=');
        if (pos == std::string::npos) {
            return "export: " + arg + ": invalid format, expected KEY=VALUE\n";
        }
        sh.set_env(arg.substr(0, pos), arg.substr(pos + 1));
    }
    sh.set_exit_code(0);
    return "";
}

std::string unset(Shell& sh, const Command& cmd) {
    if (cmd.argv.size() < 2) {
        return "unset: missing operand\n";
    }
    for (size_t i = 1; i < cmd.argv.size(); ++i) {
        sh.unset_env(cmd.argv[i]);
    }
    sh.set_exit_code(0);
    return "";
}

} // namespace Builtins
