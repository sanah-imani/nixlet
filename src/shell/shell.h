#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

struct Command {
    std::vector<std::string> argv;
    std::string              stdin_file;
    std::string              stdout_file;
    std::string              stdout_append;
};

// Forward declare so shell.h doesn't need to include parser.h
struct Pipeline;
class  Parser;

class Shell {
public:
    Shell();

    void init();

    std::string execute(const std::string& line);

    const std::string& cwd() const { return _cwd; }
    void               set_cwd(const std::string& cwd) { _cwd = cwd; }

    const std::string& get_env(const std::string& key) const;
    void               set_env(const std::string& key, const std::string& val);
    void               unset_env(const std::string& key);
    const std::unordered_map<std::string, std::string>& env() const { return _env; }

    void set_exit_code(int n) { _last_exit = n; }
    int  last_exit_code() const { return _last_exit; }

private:
    using BuiltinFn = std::function<std::string(Shell&, const Command&)>;

    std::string execute_pipeline(const Pipeline& pl);
    std::string expand_vars(const std::string& s) const;
    std::string dispatch(const Command& cmd);

    std::string                                  _cwd;
    std::unordered_map<std::string, std::string> _env;
    std::unordered_map<std::string, BuiltinFn>   _builtins;
    int                                          _last_exit = 0;
};
