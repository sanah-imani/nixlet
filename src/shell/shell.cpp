#include "shell.h"
#include "parser.h"
#include "builtins.h"
#include "../fs/vfs.h"
#include "../utils/util.h"
#include "../process/process.h"
#include <cctype>

static Parser g_parser;

Shell::Shell() {
    _builtins["cd"]     = Builtins::cd;
    _builtins["pwd"]    = Builtins::pwd;
    _builtins["echo"]   = Builtins::echo;
    _builtins["exit"]   = Builtins::exit_;
    _builtins["env"]    = Builtins::env;
    _builtins["export"] = Builtins::export_;
    _builtins["unset"]  = Builtins::unset;
    _builtins["ls"]     = Utils::ls;
    _builtins["cat"]    = Utils::cat;
    _builtins["touch"]  = Utils::touch;
    _builtins["mkdir"]  = Utils::mkdir;
    _builtins["rm"]     = Utils::rm;
    _builtins["cp"]     = Utils::cp;
    _builtins["mv"]     = Utils::mv;
    _builtins["grep"]   = Utils::grep;
    _builtins["find"]   = Utils::find;
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

    Pipeline pl = g_parser.parse(trimmed);

    if (!g_parser.error().empty())
        return "nixlet: " + g_parser.error() + "\n";

    return execute_pipeline(pl);
}

std::string Shell::execute_pipeline(const Pipeline& pl) {
    bool has_pipe = false;
    for (const auto& op: pl.ops)
        if (op == ChainOp::Pipe) { has_pipe = true; break;}
    
    if (has_pipe)
        return run_pipeline(*this, pl);

    std::string out;

    for (size_t i = 0; i < pl.commands.size(); ++i) {
        Command cmd = pl.commands[i];

        // Expand $VAR in argv for Word tokens (WordNoExpand already filtered
        // by parser — single-quoted values arrive verbatim and are not expanded)
        for (auto& arg : cmd.argv)
            arg = expand_vars(arg);

        // && — skip if previous command failed
        if (i > 0 && pl.ops[i-1] == ChainOp::And && _last_exit != 0)
            break;

        // || — skip if previous command succeeded
        if (i > 0 && pl.ops[i-1] == ChainOp::Or && _last_exit == 0)
            break;

        // Semicolon — always run, no condition

        if (cmd.argv.empty()) continue;

        // Handle stdout redirect for this command
        std::string cmd_out = dispatch(cmd);

        if (!cmd.stdout_file.empty()) {
            VFS::get().write(cmd.stdout_file, _cwd, cmd_out, false);
            cmd_out = "";
        } else if (!cmd.stdout_append.empty()) {
            VFS::get().write(cmd.stdout_append, _cwd, cmd_out, true);
            cmd_out = "";
        }

        out += cmd_out;
    }

    return out;
}

std::string Shell::dispatch(const Command& cmd) {
    if (cmd.argv.empty()) return "";
    auto it = _builtins.find(cmd.argv[0]);
    if (it != _builtins.end())
        return it->second(*this, cmd);
    _last_exit = 127;
    return cmd.argv[0] + ": command not found\n";
}
