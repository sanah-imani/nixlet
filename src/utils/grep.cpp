#include "util.h"
#include "../fs/vfs.h"
#include "../fs/path.h"
#include <regex>
#include <sstream>

namespace Utils {

static std::string search_file(const std::string& content,
                                const std::string& filepath,
                                const std::regex& pattern,
                                bool show_line_numbers,
                                bool show_filename) {
    std::string out;
    std::istringstream stream(content);
    std::string line;
    int lineno = 0;
    while (std::getline(stream, line)) {
        ++lineno;
        if (!std::regex_search(line, pattern)) continue;
        if (show_filename)    out += filepath + ":";
        if (show_line_numbers) out += std::to_string(lineno) + ":";
        out += line + "\n";
    }
    return out;
}

static std::string search_dir(const std::string& dir,
                               const std::string& cwd,
                               const std::regex& pattern,
                               bool show_line_numbers) {
    std::string out;
    for (Inode* node : VFS::get().readdir(dir, cwd)) {
        const std::string full = Path::join(dir, node->name);
        if (node->is_dir()) {
            out += search_dir(full, cwd, pattern, show_line_numbers);
        } else if (node->is_file()) {
            auto data = VFS::get().read(full, cwd);
            if (data) out += search_file(*data, full, pattern, show_line_numbers, true);
        }
    }
    return out;
}

std::string grep(Shell& sh, const Command& cmd) {
    bool flag_n = false;
    bool flag_i = false;
    bool flag_r = false;
    std::string pattern_str;
    std::vector<std::string> files;

    for (size_t i = 1; i < cmd.argv.size(); ++i) {
        const auto& arg = cmd.argv[i];
        if (!arg.empty() && arg[0] == '-') {
            for (size_t j = 1; j < arg.size(); ++j) {
                if (arg[j] == 'n') flag_n = true;
                if (arg[j] == 'i') flag_i = true;
                if (arg[j] == 'r') flag_r = true;
            }
        } else if (pattern_str.empty()) {
            pattern_str = arg;
        } else {
            files.push_back(arg);
        }
    }

    if (pattern_str.empty()) {
        sh.set_exit_code(1);
        return "grep: missing pattern\n";
    }

    auto flags = flag_i
        ? std::regex::ECMAScript | std::regex::icase
        : std::regex::ECMAScript;
    std::regex pattern;
    try { pattern = std::regex(pattern_str, flags); }
    catch (const std::regex_error&) {
        sh.set_exit_code(1);
        return "grep: invalid pattern: " + pattern_str + "\n";
    }

    std::string out;
    if (flag_r) {
        out = search_dir(files.empty() ? sh.cwd() : files[0], sh.cwd(), pattern, flag_n);
    } else {
        if (files.empty()) {
            if (!cmd.stdin_data.empty()){
                out = search_file(cmd.stdin_data,"(stdin)", pattern, flag_n, false);
            } else { 
                sh.set_exit_code(1);
                return "grep: missing file operand\n";
            }
        }
        bool multi_file = files.size() > 1;
        for (const auto& file : files) {
            auto data = VFS::get().read(file, sh.cwd());
            if (!data) {
                sh.set_exit_code(1);
                return "grep: " + file + ": " + VFS::get().last_error() + "\n";
            }
            out += search_file(*data, file, pattern, flag_n, multi_file);
        }
    }

    sh.set_exit_code(out.empty() ? 1 : 0);
    return out;
}

} // namespace Utils
