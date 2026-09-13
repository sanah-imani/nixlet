#pragma once
#include "../shell/parser.h"
#include <string>
#include <vector>

struct Process {
    std::vector<std::string> argv;
    std::string stdin_data;
    std::string stdout_data;
    int exit_code = 0;
};

class Shell;
std::string run_pipeline(Shell& sh, const Pipeline& pl);


