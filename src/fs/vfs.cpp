#include "vfs.h"
#include "path.h"
#include <algorithm>
#include <sstream>
#include <ctime>

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
VFS& VFS::get() {
    static VFS instance;
    return instance;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
uint64_t VFS::now() {
    return static_cast<uint64_t>(std::time(nullptr));
}

bool VFS::err(const std::string& msg) const {
    _last_error = msg;
    return false;
}

Inode* VFS::err_null(const std::string& msg) const {
    _last_error = msg;
    return nullptr;
}

Inode* VFS::alloc_inode(InodeType type, const std::string& name, uint32_t mode) {
    auto node      = std::make_unique<Inode>();
    node->ino      = _next_ino++;
    node->type     = type;
    node->name     = name;
    node->mode     = mode;
    node->atime    = now();
    node->mtime    = now();
    node->ctime    = now();
    Inode* raw     = node.get();
    _inodes[raw->ino] = std::move(node);
    return raw;
}

void VFS::link_child(Inode* parent, Inode* child) {
    child->parent = parent;
    parent->children[child->name] = child;
}

void VFS::unlink_child(Inode* child) {
    if (child->parent) {
        child->parent->children.erase(child->name);
        child->parent = nullptr;
    }
}

void VFS::free_subtree(Inode* node) {
    if (!node) return;
    unlink_child(node);
    free_subtree_recursive(node);
}

void VFS::free_subtree_recursive(Inode* node) {
    for (auto& [name, child] : node->children) {
        free_subtree_recursive(child);
    }
    node->children.clear();
    _inodes.erase(node->ino);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void VFS::init() {
    reset();

    // Build:  /  /tmp  /home  /home/user
    _root = alloc_inode(InodeType::Directory, "", 0755);

    Inode* tmp  = alloc_inode(InodeType::Directory, "tmp",  01777);
    Inode* home = alloc_inode(InodeType::Directory, "home", 0755);
    Inode* user = alloc_inode(InodeType::Directory, "user", 0755);

    link_child(_root, tmp);
    link_child(_root, home);
    link_child(home,  user);

    _last_error.clear();
}

void VFS::reset() {
    _inodes.clear();
    _root     = nullptr;
    _next_ino = 1;
    _last_error.clear();
}

// ---------------------------------------------------------------------------
// Path resolution
// ---------------------------------------------------------------------------
Inode* VFS::resolve(const std::string& path, const std::string& cwd,
                    int symlink_depth) const {
    if (!_root) return err_null("Filesystem not initialised");

    const std::string abs = Path::make_absolute(path, cwd);
    if (abs == "/") return _root;

    const auto parts = Path::split(abs);
    Inode* cur = _root;

    for (size_t i = 0; i < parts.size(); ++i) {
        const std::string& part = parts[i];

        if (!cur->is_dir())
            return err_null("Not a directory: " + part);

        auto it = cur->children.find(part);
        if (it == cur->children.end())
            return err_null("No such file or directory: " + part);

        Inode* next = it->second;

        // Follow symlinks for every component (including the last)
        if (next->is_symlink()) {
            if (symlink_depth >= MAX_SYMLINK_DEPTH)
                return err_null("ELOOP: too many levels of symbolic links");

            // Resolve target relative to the symlink's parent directory
            std::string link_dir = "/";
            for (size_t j = 0; j < i; ++j) {
                link_dir = Path::join(link_dir, parts[j]);
            }
            const std::string target_abs =
                Path::make_absolute(next->link_target(), link_dir);

            // Append any remaining path components onto the resolved target
            std::string full_target = target_abs;
            for (size_t j = i + 1; j < parts.size(); ++j) {
                full_target = Path::join(full_target, parts[j]);
            }

            return resolve(full_target, "/", symlink_depth + 1);
        }

        cur = next;
    }

    _last_error.clear();
    return cur;
}

Inode* VFS::resolve_parent(const std::string& path, const std::string& cwd,
                            std::string& out_name) const {
    const std::string abs = Path::make_absolute(path, cwd);
    out_name = Path::basename(abs);
    const std::string parent_path = Path::dirname(abs);
    return resolve(parent_path, "/");
}

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------
bool VFS::create(const std::string& path, const std::string& cwd, uint32_t mode) {
    std::string name;
    Inode* parent = resolve_parent(path, cwd, name);
    if (!parent)          return err("No such file or directory: " + Path::dirname(path));
    if (!parent->is_dir()) return err("Not a directory");
    if (parent->children.count(name)) return err("File exists: " + name);

    Inode* f = alloc_inode(InodeType::File, name, mode);
    link_child(parent, f);
    return true;
}

bool VFS::write(const std::string& path, const std::string& cwd,
                const std::string& data, bool append) {
    Inode* node = resolve(path, cwd);
    if (!node) {
        // Create on first write
        if (!create(path, cwd)) return false;
        node = resolve(path, cwd);
        if (!node) return false;
    }
    if (!node->is_file()) return err("Is a directory: " + path);

    if (append) node->content += data;
    else        node->content  = data;

    node->mtime = now();
    return true;
}

std::optional<std::string> VFS::read(const std::string& path, const std::string& cwd) {
    Inode* node = resolve(path, cwd);
    if (!node) return std::nullopt;
    if (!node->is_file()) { err("Is a directory: " + path); return std::nullopt; }
    node->atime = now();
    return node->content;
}

bool VFS::unlink(const std::string& path, const std::string& cwd) {
    std::string name;
    Inode* parent = resolve_parent(path, cwd, name);  
    if (!parent) return false;
    auto it = parent->children.find(name);
    if (it == parent->children.end()) return err("No such file or directory: " + name);
    Inode* node = it->second;
    if (!node) return false;
    if (node->is_dir()) return err("Is a directory: " + path);

    unlink_child(node);
    _inodes.erase(node->ino);
    return true;
}

// ---------------------------------------------------------------------------
// Directory operations
// ---------------------------------------------------------------------------
bool VFS::mkdir(const std::string& path, const std::string& cwd,
                bool parents, uint32_t mode) {
    const std::string abs = Path::make_absolute(path, cwd);

    if (parents) {
        // Walk components and create any missing ones
        const auto parts = Path::split(abs);
        Inode* cur = _root;
        std::string built = "/";
        for (const auto& part : parts) {
            auto it = cur->children.find(part);
            if (it == cur->children.end()) {
                Inode* d = alloc_inode(InodeType::Directory, part, mode);
                link_child(cur, d);
                cur = d;
            } else {
                if (!it->second->is_dir())
                    return err("Not a directory: " + Path::join(built, part));
                cur = it->second;
            }
            built = Path::join(built, part);
        }
        return true;
    }

    std::string name;
    Inode* parent = resolve_parent(abs, "/", name);
    if (!parent)           return err("No such file or directory");
    if (!parent->is_dir()) return err("Not a directory");
    if (parent->children.count(name)) return err("File exists: " + name);

    Inode* d = alloc_inode(InodeType::Directory, name, mode);
    link_child(parent, d);
    return true;
}

bool VFS::rmdir(const std::string& path, const std::string& cwd, bool recursive) {
    Inode* node = resolve(path, cwd);
    if (!node)             return false;
    if (!node->is_dir())   return err("Not a directory: " + path);
    if (node == _root)     return err("Cannot remove root directory");

    if (!recursive && !node->children.empty())
        return err("Directory not empty: " + path);

    free_subtree(node);
    return true;
}

std::vector<Inode*> VFS::readdir(const std::string& path, const std::string& cwd) {
    Inode* node = resolve(path, cwd);
    if (!node || !node->is_dir()) {
        if (node) err("Not a directory: " + path);
        return {};
    }
    std::vector<Inode*> result;
    result.reserve(node->children.size());
    for (auto& [name, child] : node->children) {
        result.push_back(child);
    }
    // std::map is already sorted by key, so result is alphabetical
    return result;
}

// ---------------------------------------------------------------------------
// Symlink operations
// ---------------------------------------------------------------------------
bool VFS::symlink(const std::string& target, const std::string& link_path,
                  const std::string& cwd) {
    std::string name;
    Inode* parent = resolve_parent(link_path, cwd, name);
    if (!parent)           return err("No such file or directory");
    if (!parent->is_dir()) return err("Not a directory");
    if (parent->children.count(name)) return err("File exists: " + name);

    Inode* sym = alloc_inode(InodeType::Symlink, name, 0777);
    sym->content = target;   // store raw target
    link_child(parent, sym);
    return true;
}

std::optional<std::string> VFS::readlink(const std::string& path,
                                          const std::string& cwd) {
    // Use lstat-style resolution — do NOT follow the final component
    std::string name;
    Inode* parent = resolve_parent(path, cwd, name);
    if (!parent) return std::nullopt;

    auto it = parent->children.find(name);
    if (it == parent->children.end()) {
        err("No such file or directory: " + name);
        return std::nullopt;
    }
    Inode* node = it->second;
    if (!node->is_symlink()) {
        err("EINVAL: not a symbolic link: " + name);
        return std::nullopt;
    }
    return node->link_target();
}

// ---------------------------------------------------------------------------
// Universal operations
// ---------------------------------------------------------------------------
bool VFS::rename(const std::string& src, const std::string& dst,
                 const std::string& cwd) {
    std::string src_name;
    Inode* src_parent = resolve_parent(src, cwd, src_name);
    if (!src_parent) return false;
    auto src_it = src_parent->children.find(src_name);
    if (src_it == src_parent->children.end()) return err("No such file or directory: " + src_name);
    Inode* src_node = src_it->second; 
    if (!src_node) return false;

    std::string dst_name;
    Inode* dst_parent = resolve_parent(dst, cwd, dst_name);
    if (!dst_parent)           return err("No such file or directory");
    if (!dst_parent->is_dir()) return err("Not a directory");

    // If dst is an existing directory, move src inside it
    auto it = dst_parent->children.find(dst_name);
    if (it != dst_parent->children.end() && it->second->is_dir()) {
        dst_parent = it->second;
        dst_name   = src_node->name;
    }

    // Refuse to clobber a non-empty directory with a file
    auto existing = dst_parent->children.find(dst_name);
    if (existing != dst_parent->children.end()) {
        Inode* ev = existing->second;
        if (ev->is_dir() && !ev->children.empty())
            return err("Directory not empty: " + dst_name);
        // Remove existing target
        unlink_child(ev);
        _inodes.erase(ev->ino);
    }

    unlink_child(src_node);
    src_node->name = dst_name;
    src_node->ctime = now();
    link_child(dst_parent, src_node);
    return true;
}

bool VFS::copy_subtree(Inode* src, Inode* dst_parent, const std::string& new_name) {
    Inode* dst = alloc_inode(src->type, new_name, src->mode);
    dst->content = src->content;
    dst->mtime   = src->mtime;
    link_child(dst_parent, dst);

    for (auto& [child_name, child] : src->children) {
        if (!copy_subtree(child, dst, child_name)) return false;
    }
    return true;
}

bool VFS::copy(const std::string& src, const std::string& dst,
               const std::string& cwd, bool recursive) {
    Inode* src_node = resolve(src, cwd);
    if (!src_node) return false;

    if (src_node->is_dir() && !recursive)
        return err("Omitting directory: " + src + " (use -r)");

    std::string dst_name;
    Inode* dst_parent = resolve_parent(dst, cwd, dst_name);
    if (!dst_parent)           return err("No such file or directory");
    if (!dst_parent->is_dir()) return err("Not a directory");

    // If dst is an existing directory, copy src inside it
    auto it = dst_parent->children.find(dst_name);
    if (it != dst_parent->children.end() && it->second->is_dir()) {
        dst_parent = it->second;
        dst_name   = src_node->name;
    }

    // Remove existing file at dst (not dir)
    auto existing = dst_parent->children.find(dst_name);
    if (existing != dst_parent->children.end()) {
        Inode* ev = existing->second;
        if (ev->is_dir()) return err("Cannot overwrite directory with file");
        unlink_child(ev);
        _inodes.erase(ev->ino);
    }

    return copy_subtree(src_node, dst_parent, dst_name);
}

// ---------------------------------------------------------------------------
// stat / lstat
// ---------------------------------------------------------------------------
std::optional<StatResult> VFS::stat_impl(const std::string& path,
                                          const std::string& cwd, bool follow) {
    Inode* node = nullptr;
    if (follow) {
        node = resolve(path, cwd);
    } else {
        // Resolve everything except the final symlink
        std::string name;
        Inode* parent = resolve_parent(path, cwd, name);
        if (!parent) return std::nullopt;
        auto it = parent->children.find(name);
        if (it == parent->children.end()) {
            err("No such file or directory: " + name);
            return std::nullopt;
        }
        node = it->second;
    }
    if (!node) return std::nullopt;

    return StatResult{node->ino, node->type, node->mode, node->size(), node->mtime};
}

std::optional<StatResult> VFS::stat(const std::string& path, const std::string& cwd) {
    return stat_impl(path, cwd, /*follow=*/true);
}

std::optional<StatResult> VFS::lstat(const std::string& path, const std::string& cwd) {
    return stat_impl(path, cwd, /*follow=*/false);
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

// Minimal hand-rolled JSON escaping (no external deps)
static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string VFS::serialize() const {
    std::string out;
    out.reserve(4096);
    out += "{\"next_ino\":";
    out += std::to_string(_next_ino);
    out += ",\"inodes\":[";

    bool first = true;
    for (auto& [ino, node] : _inodes) {
        if (!first) out += ',';
        first = false;

        out += "{\"ino\":";
        out += std::to_string(node->ino);
        out += ",\"type\":\"";
        if      (node->is_file())    out += "file";
        else if (node->is_dir())     out += "dir";
        else                          out += "symlink";
        out += "\",\"name\":\"";
        out += json_escape(node->name);
        out += "\",\"mode\":";
        out += std::to_string(node->mode);
        out += ",\"parent\":";
        out += std::to_string(node->parent ? node->parent->ino : 0);
        out += ",\"mtime\":";
        out += std::to_string(node->mtime);
        out += ",\"content\":\"";
        out += json_escape(node->content);
        out += "\",\"children\":[";

        bool fc = true;
        for (auto& [cname, child] : node->children) {
            if (!fc) out += ',';
            fc = false;
            out += std::to_string(child->ino);
        }
        out += "]}";
    }

    out += "]}";
    return out;
}

// ---------------------------------------------------------------------------
// Deserialize — minimal hand-rolled JSON parser for our own format only.
//
// Strategy:
//   1. Parse next_ino and the inodes array.
//   2. First pass: allocate all Inode objects (no links yet).
//   3. Second pass: wire parent/children pointers.
// ---------------------------------------------------------------------------

// Tiny helpers for the parser
static void skip_ws(const std::string& s, size_t& i) {
    while (i < s.size() && std::isspace((unsigned char)s[i])) ++i;
}

static bool expect(const std::string& s, size_t& i, char c) {
    skip_ws(s, i);
    if (i >= s.size() || s[i] != c) return false;
    ++i; return true;
}

static std::string parse_string(const std::string& s, size_t& i) {
    skip_ws(s, i);
    if (i >= s.size() || s[i] != '"') return "";
    ++i;
    std::string out;
    while (i < s.size() && s[i] != '"') {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            switch (s[i]) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
        ++i;
    }
    if (i < s.size()) ++i; // consume closing '"'
    return out;
}

static uint64_t parse_uint(const std::string& s, size_t& i) {
    skip_ws(s, i);
    uint64_t v = 0;
    while (i < s.size() && std::isdigit((unsigned char)s[i])) {
        v = v * 10 + (s[i++] - '0');
    }
    return v;
}

bool VFS::deserialize(const std::string& json) {
    reset();
    size_t i = 0;

    // { "next_ino": N, "inodes": [ ... ] }
    if (!expect(json, i, '{')) return false;

    // Parse key-value pairs at top level
    struct RawInode {
        uint32_t            ino;
        InodeType           type;
        std::string         name;
        uint32_t            mode;
        uint32_t            parent_ino;
        uint64_t            mtime;
        std::string         content;
        std::vector<uint32_t> child_inos;
    };
    std::vector<RawInode> raw_inodes;

    while (i < json.size() && json[i] != '}') {
        skip_ws(json, i);
        if (json[i] == ',') { ++i; continue; }

        std::string key = parse_string(json, i);
        if (!expect(json, i, ':')) return false;
        skip_ws(json, i);

        if (key == "next_ino") {
            _next_ino = static_cast<uint32_t>(parse_uint(json, i));
        } else if (key == "inodes") {
            if (!expect(json, i, '[')) return false;
            while (i < json.size() && json[i] != ']') {
                skip_ws(json, i);
                if (json[i] == ',') { ++i; continue; }
                if (json[i] != '{') break;
                ++i; // consume '{'

                RawInode ri{};
                while (i < json.size() && json[i] != '}') {
                    skip_ws(json, i);
                    if (json[i] == ',') { ++i; continue; }
                    std::string k = parse_string(json, i);
                    if (!expect(json, i, ':')) return false;
                    skip_ws(json, i);

                    if      (k == "ino")     ri.ino        = static_cast<uint32_t>(parse_uint(json, i));
                    else if (k == "mode")    ri.mode       = static_cast<uint32_t>(parse_uint(json, i));
                    else if (k == "parent")  ri.parent_ino = static_cast<uint32_t>(parse_uint(json, i));
                    else if (k == "mtime")   ri.mtime      = parse_uint(json, i);
                    else if (k == "name")    ri.name       = parse_string(json, i);
                    else if (k == "content") ri.content    = parse_string(json, i);
                    else if (k == "type") {
                        std::string t = parse_string(json, i);
                        if      (t == "dir")     ri.type = InodeType::Directory;
                        else if (t == "symlink") ri.type = InodeType::Symlink;
                        else                      ri.type = InodeType::File;
                    } else if (k == "children") {
                        if (!expect(json, i, '[')) return false;
                        while (i < json.size() && json[i] != ']') {
                            skip_ws(json, i);
                            if (json[i] == ',') { ++i; continue; }
                            ri.child_inos.push_back(
                                static_cast<uint32_t>(parse_uint(json, i)));
                        }
                        if (!expect(json, i, ']')) return false;
                    } else {
                        // Unknown key — skip value (string or number only)
                        if (json[i] == '"') parse_string(json, i);
                        else parse_uint(json, i);
                    }
                }
                if (!expect(json, i, '}')) return false;
                raw_inodes.push_back(std::move(ri));
            }
            if (!expect(json, i, ']')) return false;
        } else {
            // Unknown top-level key — skip
            if (json[i] == '"') parse_string(json, i);
            else parse_uint(json, i);
        }
        skip_ws(json, i);
    }

    // ── Pass 1: allocate Inode objects ──────────────────────────────────────
    for (auto& ri : raw_inodes) {
        auto node      = std::make_unique<Inode>();
        node->ino      = ri.ino;
        node->type     = ri.type;
        node->name     = ri.name;
        node->mode     = ri.mode;
        node->mtime    = ri.mtime;
        node->content  = ri.content;
        _inodes[ri.ino] = std::move(node);
    }

    // ── Pass 2: wire pointers ───────────────────────────────────────────────
    for (auto& ri : raw_inodes) {
        Inode* node = _inodes.at(ri.ino).get();

        if (ri.parent_ino == 0) {
            // This is root
            _root = node;
        } else {
            auto pit = _inodes.find(ri.parent_ino);
            if (pit != _inodes.end()) {
                node->parent = pit->second.get();
            }
        }

        for (uint32_t child_ino : ri.child_inos) {
            auto cit = _inodes.find(child_ino);
            if (cit != _inodes.end()) {
                node->children[cit->second->name] = cit->second.get();
            }
        }
    }

    if (!_root) return false;
    _last_error.clear();
    return true;
}
