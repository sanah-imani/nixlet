#include "process.h"
#include "../shell/shell.h"
#include "../shell/parser.h"
#include "../fs/vfs.h"

std::string run_pipeline(Shell& sh, const Pipeline& pl) {
    const size_t count = pl.commands.size();
    std::string pipe_buf;  // stdout of process i fed as stdin of process i+1

    for (size_t i = 0; i < count; ++i) {
        Command exec_cmd = pl.commands[i];

        // Feed pipe buffer from previous process
        if (i > 0 && pl.ops[i-1] == ChainOp::Pipe)
            exec_cmd.stdin_data = pipe_buf;

        // stdin_file redirect overrides pipe input
        if (!exec_cmd.stdin_file.empty()) {
            auto file_data = VFS::get().read(exec_cmd.stdin_file, sh.cwd());
            if (!file_data)
                return exec_cmd.stdin_file + ": " + VFS::get().last_error() + "\n";
            exec_cmd.stdin_data = *file_data;
        }

        // Expand $VAR in argv
        for (auto& arg : exec_cmd.argv)
            arg = sh.expand_vars(arg);

        if (exec_cmd.argv.empty()) {
            pipe_buf.clear();
            continue;
        }

        // Run the command
        std::string cmd_out = sh.dispatch(exec_cmd);

        // stdout redirect
        if (!exec_cmd.stdout_file.empty()) {
            VFS::get().write(exec_cmd.stdout_file, sh.cwd(), cmd_out, false);
            cmd_out.clear();
        } else if (!exec_cmd.stdout_append.empty()) {
            VFS::get().write(exec_cmd.stdout_append, sh.cwd(), cmd_out, true);
            cmd_out.clear();
        }

        pipe_buf = cmd_out;
    }

    return pipe_buf;
}
