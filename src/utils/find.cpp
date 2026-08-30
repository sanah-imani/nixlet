#include "util.h"
#include "../fs/vfs.h"
#include "../fs/path.h"
#include <regex>

namespace Utils {

static std::string glob_to_regex(const std::string& glob) {
    std::string re;
    for (char c : glob) {
        switch (c) {
            case '*': re += ".*";  break;
            case '?': re += '.';   break;
            case '.': re += "\\."; break;
            case '(': re += "\\("; break;
            case ')': re += "\\)"; break;
            case '[': re += "\\["; break;
            case ']': re += "\\]"; break;
            case '^': re += "\\^"; break;
            case '$': re += "\\$"; break;
            default:  re += c;     break;
        }
    }
    return "^" + re + "$";
}

static void walk(const std::string& dir,
                 const std::string& cwd,
                 const std::regex* name_pattern,
                 std::string& out) {
    for (Inode* node : VFS::get().readdir(dir, cwd)) {
        const std::string full = Path::join(dir, node->name);
        if (!name_pattern || std::regex_match(node->name, *name_pattern))
            out += full + "\n";
        if (node->is_dir()) walk(full, cwd, name_pattern, out);
    }
}

std::string find(Shell& sh, const Command& cmd) {
    std::string root;
    std::string name_glob;

    for (size_t i = 1; i < cmd.argv.size(); ++i) {
        if (cmd.argv[i] == "-name" && i + 1 < cmd.argv.size())
            name_glob = cmd.argv[++i];
        else if (root.empty())
            root = cmd.argv[i];
    }

    if (root.empty()) root = sh.cwd();

    std::regex name_pattern;
    bool has_name = !name_glob.empty();
    if (has_name) {
        try { name_pattern = std::regex(glob_to_regex(name_glob)); }
        catch (const std::regex_error&) {
            sh.set_exit_code(1);
            return "find: invalid -name pattern\n";
        }
    }

    auto entry = VFS::get().stat(root, sh.cwd());
    if (!entry) {
        sh.set_exit_code(1);
        return "find: " + root + ": " + VFS::get().last_error() + "\n";
    }

    std::string out;
    if (entry->type != InodeType::Directory) {
        if (!has_name || std::regex_match(Path::basename(root), name_pattern))
            out += root + "\n";
    } else {
        walk(root, sh.cwd(), has_name ? &name_pattern : nullptr, out);
    }

    sh.set_exit_code(0);
    return out;
}

} // namespace Utils
