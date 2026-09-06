#include "parser.h"
#include <cctype>

std::vector<Parser::Token> Parser::tokenize(const std::string& line) const {
    std::vector<Token> tokens;
    size_t i = 0;
    const size_t n = line.size();

    auto skip_spaces = [&]() {
        while (i < n && std::isspace((unsigned char)line[i])) ++i;
    };

    while (i < n) {
        skip_spaces();
        if (i >= n) break;

        if (line[i] == '#') break;

        // Two-char operators — must be checked before their single-char prefixes
        if (i + 1 < n) {
            if (line[i] == '&' && line[i+1] == '&') {
                tokens.push_back({ TokKind::And, "" });
                i += 2; continue;
            }
            if (line[i] == '|' && line[i+1] == '|') {
                tokens.push_back({ TokKind::Or, "" });
                i += 2; continue;
            }
            if (line[i] == '>' && line[i+1] == '>') {
                tokens.push_back({ TokKind::RedirApp, "" });
                i += 2; continue;
            }
        }

        // Single-char operators
        switch (line[i]) {
            case '|': tokens.push_back({ TokKind::Pipe,       "" }); ++i; continue;
            case '&': tokens.push_back({ TokKind::Background, "" }); ++i; continue;
            case '>': tokens.push_back({ TokKind::RedirOut,   "" }); ++i; continue;
            case '<': tokens.push_back({ TokKind::RedirIn,    "" }); ++i; continue;
            case ';': tokens.push_back({ TokKind::Semi,       "" }); ++i; continue;
        }

        // Double-quoted string — backslash escapes honoured, $VAR expansion deferred
        if (line[i] == '"') {
            ++i;
            std::string word;
            while (i < n && line[i] != '"') {
                if (line[i] == '\\' && i + 1 < n) { ++i; word += line[i]; }
                else word += line[i];
                ++i;
            }
            if (i < n) ++i; // consume closing "
            tokens.push_back({ TokKind::Word, word });
            continue;
        }

        // Single-quoted string — no escapes, no expansion
        if (line[i] == '\'') {
            ++i;
            std::string word;
            while (i < n && line[i] != '\'') word += line[i++];
            if (i < n) ++i; // consume closing '
            tokens.push_back({ TokKind::WordNoExpand, word });
            continue;
        }

        // Unquoted word — backslash escapes honoured
        std::string word;
        while (i < n
               && !std::isspace((unsigned char)line[i])
               && line[i] != '|' && line[i] != '&'
               && line[i] != '>' && line[i] != '<'
               && line[i] != ';' && line[i] != '"'
               && line[i] != '\'' && line[i] != '#') {
            if (line[i] == '\\' && i + 1 < n) { ++i; word += line[i]; }
            else word += line[i];
            ++i;
        }
        if (!word.empty()) tokens.push_back({ TokKind::Word, word });
    }

    tokens.push_back({ TokKind::End, "" });
    return tokens;
}

Command Parser::parse_command(const std::vector<Token>& tokens, size_t& pos) const {
    Command cmd;

    while (pos < tokens.size()) {
        const Token& tok = tokens[pos];

        switch (tok.kind) {
            case TokKind::End:
            case TokKind::Pipe:
            case TokKind::And:
            case TokKind::Or:
            case TokKind::Semi:
            case TokKind::Background:
                return cmd;

            case TokKind::Word:
            case TokKind::WordNoExpand:
                cmd.argv.push_back(tok.value);
                ++pos;
                break;

            case TokKind::RedirIn:
                ++pos;
                if (pos >= tokens.size()
                    || (tokens[pos].kind != TokKind::Word
                        && tokens[pos].kind != TokKind::WordNoExpand)) {
                    _error = "syntax error: expected filename after '<'";
                    return cmd;
                }
                cmd.stdin_file = tokens[pos++].value;
                break;

            case TokKind::RedirOut:
                ++pos;
                if (pos >= tokens.size()
                    || (tokens[pos].kind != TokKind::Word
                        && tokens[pos].kind != TokKind::WordNoExpand)) {
                    _error = "syntax error: expected filename after '>'";
                    return cmd;
                }
                cmd.stdout_file = tokens[pos++].value;
                break;

            case TokKind::RedirApp:
                ++pos;
                if (pos >= tokens.size()
                    || (tokens[pos].kind != TokKind::Word
                        && tokens[pos].kind != TokKind::WordNoExpand)) {
                    _error = "syntax error: expected filename after '>>'";
                    return cmd;
                }
                cmd.stdout_append = tokens[pos++].value;
                break;
        }
    }
    return cmd;
}

Pipeline Parser::parse(const std::string& line) const {
    _error.clear();
    Pipeline result;

    auto tokens = tokenize(line);
    size_t pos = 0;

    while (true) {
        Command cmd = parse_command(tokens, pos);
        result.commands.push_back(std::move(cmd));

        if (pos >= tokens.size()) break;
        const Token& sep = tokens[pos];

        if (sep.kind == TokKind::End) break;

        if (sep.kind == TokKind::Background) {
            result.background = true;
            ++pos;
            break;
        }

        ChainOp op;
        switch (sep.kind) {
            case TokKind::Pipe: op = ChainOp::Pipe;      break;
            case TokKind::And:  op = ChainOp::And;        break;
            case TokKind::Or:   op = ChainOp::Or;         break;
            case TokKind::Semi: op = ChainOp::Semicolon;  break;
            default:            goto done;
        }
        result.ops.push_back(op);
        ++pos;
    }

done:
    return result;
}
