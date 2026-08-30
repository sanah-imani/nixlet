#pragma once
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Path — pure utility functions, no state, no VFS dependency.
//
// All functions treat paths as POSIX strings ('/'-separated).
// None of them touch the VFS or the real filesystem.
// ---------------------------------------------------------------------------
namespace Path {

    // Collapse ".", "..", and redundant slashes.
    // make_absolute() should be called first if the path may be relative.
    // e.g.  "/foo/../bar//baz" → "/bar/baz"
    //        "a/./b"           → "a/b"
    std::string normalize(const std::string& path);

    // If path starts with '/', return normalize(path).
    // Otherwise join with cwd first: normalize(cwd + "/" + path).
    std::string make_absolute(const std::string& path, const std::string& cwd);

    // Split a normalized absolute path into components (no empty strings).
    // e.g. "/foo/bar/baz" → ["foo", "bar", "baz"]
    //      "/"            → []
    std::vector<std::string> split(const std::string& path);

    // Join two path segments, inserting '/' as needed.
    // e.g. join("/foo", "bar")  → "/foo/bar"
    //      join("/foo/", "/bar") → "/foo/bar"  (no double slash)
    std::string join(const std::string& a, const std::string& b);

    // Last component of a path.
    // e.g. basename("/foo/bar") → "bar"
    //      basename("/foo/")    → "foo"
    //      basename("/")        → "/"
    std::string basename(const std::string& path);

    // Everything except the last component.
    // e.g. dirname("/foo/bar") → "/foo"
    //      dirname("/foo")     → "/"
    //      dirname("/")        → "/"
    std::string dirname(const std::string& path);

} // namespace Path
