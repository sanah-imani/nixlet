#include "path.h"
#include <sstream>
#include <algorithm>

namespace Path {

// ---------------------------------------------------------------------------
// normalize
// ---------------------------------------------------------------------------
std::string normalize(const std::string& path) {
    if (path.empty()) return "/";

    const bool absolute = (path[0] == '/');
    std::vector<std::string> parts;

    std::stringstream ss(path);
    std::string tok;
    while (std::getline(ss, tok, '/')) {
        if (tok.empty() || tok == ".") {
            // skip empty segments (double slashes) and dot
            continue;
        } else if (tok == "..") {
            if (!parts.empty()) parts.pop_back();
            // at root, ".." stays at root — just don't go negative
        } else {
            parts.push_back(tok);
        }
    }

    std::string result;
    if (absolute) result += '/';
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += '/';
        result += parts[i];
    }

    if (result.empty()) return "/";
    return result;
}

// ---------------------------------------------------------------------------
// make_absolute
// ---------------------------------------------------------------------------
std::string make_absolute(const std::string& path, const std::string& cwd) {
    if (!path.empty() && path[0] == '/') {
        return normalize(path);
    }
    return normalize(cwd + "/" + path);
}

// ---------------------------------------------------------------------------
// split
// ---------------------------------------------------------------------------
std::vector<std::string> split(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string tok;
    while (std::getline(ss, tok, '/')) {
        if (!tok.empty()) parts.push_back(tok);
    }
    return parts;
}

// ---------------------------------------------------------------------------
// join
// ---------------------------------------------------------------------------
std::string join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;

    // If b is absolute, it wins
    if (b[0] == '/') return normalize(b);

    if (a.back() == '/') return normalize(a + b);
    return normalize(a + "/" + b);
}

// ---------------------------------------------------------------------------
// basename
// ---------------------------------------------------------------------------
std::string basename(const std::string& path) {
    if (path == "/") return "/";

    // Strip trailing slash
    std::string p = path;
    while (p.size() > 1 && p.back() == '/') p.pop_back();

    auto pos = p.rfind('/');
    if (pos == std::string::npos) return p;
    return p.substr(pos + 1);
}

// ---------------------------------------------------------------------------
// dirname
// ---------------------------------------------------------------------------
std::string dirname(const std::string& path) {
    if (path == "/") return "/";

    // Strip trailing slash
    std::string p = path;
    while (p.size() > 1 && p.back() == '/') p.pop_back();

    auto pos = p.rfind('/');
    if (pos == std::string::npos) return ".";
    if (pos == 0) return "/";
    return p.substr(0, pos);
}

} // namespace Path
