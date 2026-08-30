#include "util.h"
#include "../fs/vfs.h"
#include "../fs/path.h"
#include <ctime>
#include <cstdio>

namespace Utils {

static std::string mode_str(uint32_t mode, InodeType type) {
    std::string s;
    switch (type) {
        case InodeType::Directory: s += 'd'; break;
        case InodeType::Symlink:   s += 'l'; break;
        default:                   s += '-'; break;
    }
    const char* bits = "rwxrwxrwx";
    for (int i = 8; i >= 0; --i)
        s += (mode & (1u << i)) ? bits[8 - i] : '-';
    return s;
}

static std::string fmt_time(uint64_t t) {
    char buf[16];
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm* tm = std::gmtime(&tt);
    std::strftime(buf, sizeof(buf), "%b %d %H:%M", tm);
    return buf;
}

static std::string fmt_size(uint64_t sz) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%6llu", (unsigned long long)sz);
    return buf;
}

std::string ls(Shell& sh, const Command& cmd) {
    bool flag_l = false;
    bool flag_a = false;
    std::string target;

    for (size_t i = 1; i < cmd.argv.size(); ++i) {
        const auto& arg = cmd.argv[i];
        if (!arg.empty() && arg[0] == '-') {
            for (size_t j = 1; j < arg.size(); ++j) {
                if (arg[j] == 'l') flag_l = true;
                if (arg[j] == 'a') flag_a = true;
            }
        } else {
            target = arg;
        }
    }

    const std::string path = target.empty() ? sh.cwd() : target;
    auto entries = VFS::get().readdir(path, sh.cwd());

    if (!VFS::get().last_error().empty() && entries.empty()) {
        sh.set_exit_code(1);
        return "ls: " + path + ": " + VFS::get().last_error() + "\n";
    }

    std::string out;

    for (Inode* node : entries) {
        if (!flag_a && !node->name.empty() && node->name[0] == '.') continue;

        if (flag_l) {
            const std::string full = Path::join(path, node->name);
            auto st = VFS::get().lstat(full, sh.cwd());
            if (!st) continue;

            out += mode_str(st->mode, st->type);
            out += "  1 user ";
            out += fmt_size(st->size);
            out += "  ";
            out += fmt_time(st->mtime);
            out += "  ";
            out += node->name;

            if (st->type == InodeType::Directory) {
                out += '/';
            } else if (st->type == InodeType::Symlink) {
                auto link_target = VFS::get().readlink(full, sh.cwd());
                if (link_target) out += " -> " + *link_target;
            }
            out += '\n';
        } else {
            out += node->name;
            if (node->is_dir())     out += '/';
            if (node->is_symlink()) out += '@';
            out += "  ";
        }
    }

    if (!flag_l && !out.empty()) out += '\n';
    sh.set_exit_code(0);
    return out;
}

} // namespace Utils
