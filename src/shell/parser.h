#pragma once
#include "shell.h"
#include <string>
#include <vector>

enum class ChainOp { Pipe, And, Or, Semicolon };

struct Pipeline {
    std::vector<Command> commands;
    std::vector<ChainOp> ops;       // ops[i] connects commands[i] → commands[i+1]
    bool                 background = false;
};

class Parser {
public:
    Pipeline           parse(const std::string& line) const;
    const std::string& error() const { return _error; }

private:
    enum class TokKind {
        Word,           // unquoted or double-quoted — subject to $VAR expansion
        WordNoExpand,   // single-quoted — passed through verbatim
        Pipe,           // |
        And,            // &&
        Or,             // ||
        Semi,           // ;
        Background,     // &
        RedirIn,        // <
        RedirOut,       // >
        RedirApp,       // >>
        End,
    };

    struct Token {
        TokKind     kind;
        std::string value;
    };

    std::vector<Token> tokenize(const std::string& line) const;
    Command            parse_command(const std::vector<Token>& tokens,
                                     size_t& pos) const;

    mutable std::string _error;
};
