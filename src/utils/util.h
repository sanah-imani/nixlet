#pragma once
#include "../shell/shell.h"

namespace Utils {
    std::string ls(Shell& sh, const Command& cmd);
    std::string cat(Shell& sh, const Command& cmd);
    std::string touch(Shell& sh, const Command& cmd);
    std::string mkdir(Shell& sh, const Command& cmd);
    std::string rm(Shell& sh, const Command& cmd);
    std::string cp(Shell& sh, const Command& cmd);
    std::string mv(Shell& sh, const Command& cmd);
    std::string grep(Shell& sh, const Command& cmd);
    std::string find(Shell& sh, const Command& cmd);
}
