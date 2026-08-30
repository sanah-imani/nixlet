#include "shell.h"
#include "builtins.h"
#include "../fs/vfs.h"
#include <cctype>

#include "../utils/util.h"

Shell::Shell() {
    _builtins["cd"]     = Builtins::cd;
    _builtins["pwd"]    = Builtins::pwd;
    _builtins["echo"]   = Builtins::echo;
    _builtins["exit"]   = Builtins::exit_;
    _builtins["env"]    = Builtins::env;
    _builtins["export"] = Builtins::export_;
    _builtins["unset"]  = Builtins::unset;
    _builtins["ls"] = Utils::ls;
    _builtins["cat"] = Utils::cat;
    _builtins["touch"] = Utils::touch;
    _builtins["mkdir"] = Utils::mkdir;
    _builtins["rm"] = Utils::rm;
    _builtins["cp"] = Utils::cp;
    _builtins["mv"] = Utils::mv;
    _builtins["grep"] = Utils::grep;
    _builtins["find"] = Utils::find;
}

void Shell::init() {
    VFS::get().init();
    _cwd       = "/home/user";
    _last_exit = 0;
    _env = {
        { "HOME",  "/home/user" },
        { "PATH",  "/bin"       },
        { "USER",  "user"       },
        { "SHELL", "nixlet"     },
        { "PWD",   "/home/user" },
    };
}

const std::string& Shell::get_env(const std::string& key) const {
    static const std::string empty;
    auto it = _env.find(key);
    return it != _env.end() ? it->second : empty;
}

void Shell::set_env(const std::string& key, const std::string& val) {
    _env[key] = val;
}

void Shell::unset_env(const std::string& key) {
    _env.erase(key);
}

Command Shell::parse_line(const std::string& line) const {
    Command cmd;
    size_t i = 0;
    const size_t n = line.size();

    auto skip_spaces = [&]() {
        while (i < n && std::isspace((unsigned char)line[i])) ++i;
    };

    while (i < n) {
        skip_spaces();
        if (i >= n) break;

        if (line[i] == '#') break;

        if (i + 1 < n && line[i] == '>' && line[i + 1] == '>') {
            i += 2;
            skip_spaces();
            std::string target;
            while (i < n && !std::isspace((unsigned char)line[i]))
                target += line[i++];
            cmd.stdout_append = target;
            continue;
        }

        if (line[i] == '>') {
            ++i;
            skip_spaces();
            std::string target;
            while (i < n && !std::isspace((unsigned char)line[i]))
                target += line[i++];
            cmd.stdout_file = target;
            continue;
        }

        if (line[i] == '<') {
            ++i;
            skip_spaces();
            std::string target;
            while (i < n && !std::isspace((unsigned char)line[i]))
                target += line[i++];
            cmd.stdin_file = target;
            continue;
        }

        if (line[i] == '"') {
            ++i;
            std::string tok;
            while (i < n && line[i] != '"')
                tok += line[i++];
            if (i < n) ++i;
            cmd.argv.push_back(tok);
            continue;
        }

        std::string tok;
        while (i < n
               && !std::isspace((unsigned char)line[i])
               && line[i] != '"'
               && line[i] != '>'
               && line[i] != '<'
               && line[i] != '#') {
            tok += line[i++];
        }
        if (!tok.empty()) cmd.argv.push_back(tok);
    }

    return cmd;
}

std::string Shell::expand_vars(const std::string& s) const {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '$' && i + 1 < s.size()) {
            ++i;
            if (s[i] == '{') {
                ++i;
                std::string key;
                while (i < s.size() && s[i] != '}') key += s[i++];
                if (i < s.size()) ++i;
                out += get_env(key);
            } else {
                std::string key;
                while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '_'))
                    key += s[i++];
                out += get_env(key);
            }
        } else {
            out += s[i++];
        }
    }
    return out;
}

std::string Shell::execute(const std::string& line) {
    size_t start = line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = line.find_last_not_of(" \t\r\n");
    const std::string trimmed = line.substr(start, end - start + 1);
    if (trimmed.empty()) return "";

    Command cmd = parse_line(trimmed);
    if (cmd.argv.empty()) return "";

    for (auto& arg : cmd.argv)
        arg = expand_vars(arg);

    return dispatch(cmd);
}

std::string Shell::dispatch(const Command& cmd) {
    auto it = _builtins.find(cmd.argv[0]);
    if (it != _builtins.end()) {
        std::string output = it->second(*this, cmd);
        return output;
    }
    _last_exit = 127;
    return cmd.argv[0] + ": command not found\n";
}
