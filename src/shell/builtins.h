#pragma once
#include "shell.h"
#include <string>

namespace Builtins {
    std::string cd(Shell& sh, const Command& cmd);
    std::string pwd(Shell& sh, const Command& cmd);
    std::string echo(Shell& sh, const Command& cmd);
    std::string exit_(Shell& sh, const Command& cmd);

    std::string env(Shell& sh, const Command& cmd);
    std::string export_(Shell& sh, const Command& cmd);
    std::string unset(Shell& sh, const Command& cmd);
}