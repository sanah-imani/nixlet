#pragma once
#include "inode.h"
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

// ---------------------------------------------------------------------------
// StatResult — returned by stat() / lstat()
// ---------------------------------------------------------------------------
struct StatResult {
    uint32_t  ino;
    InodeType type;
    uint32_t  mode;
    uint64_t  size;
    uint64_t  mtime;
};

// ---------------------------------------------------------------------------
// VFS — singleton virtual filesystem
//
// All paths are resolved relative to a caller-supplied `cwd` string.
// On failure every method sets last_error() and returns false / nullopt / nullptr.
//
// Symlink following:
//   resolve()  follows symlinks transparently (up to MAX_SYMLINK_DEPTH hops).
//   lstat()    does NOT follow the final component if it is a symlink.
//   resolve()  is called with symlink_depth > 0 during recursive link expansion;
//              callers should always use the default (0).
//
// Ownership:
//   All Inode* returned are non-owning views into VFS-managed memory.
//   They are valid until the next mutating call that affects that inode.
// ---------------------------------------------------------------------------
class VFS {
public:
    static constexpr int MAX_SYMLINK_DEPTH = 8;

    static VFS& get();   // Meyer's singleton — thread-safe in C++11+

    // ── Lifecycle ────────────────────────────────────────────────────────────

    // Build the initial directory tree: /  /tmp  /home  /home/user
    void init();

    // Wipe the entire filesystem and reinitialise (used by deserialize).
    void reset();

    // ── Path resolution ──────────────────────────────────────────────────────

    // Walk the tree to the node at `path`.  Follows symlinks.
    // Returns nullptr and sets last_error() if any component is missing or
    // if a symlink loop is detected.
    Inode* resolve(const std::string& path, const std::string& cwd,
                   int symlink_depth = 0) const;

    // Resolve the parent directory of `path`, and write the final component
    // (basename) into `out_name`.  Follows symlinks in all but the last component.
    // Returns nullptr if the parent directory does not exist.
    Inode* resolve_parent(const std::string& path, const std::string& cwd,
                          std::string& out_name) const;

    // ── File operations ──────────────────────────────────────────────────────

    // Create an empty regular file.  Fails if it already exists.
    bool create(const std::string& path, const std::string& cwd,
                uint32_t mode = 0644);

    // Overwrite (or append to) a file's content.  Creates the file if absent.
    bool write(const std::string& path, const std::string& cwd,
               const std::string& data, bool append = false);

    // Return the content of a regular file.
    std::optional<std::string> read(const std::string& path, const std::string& cwd);

    // Remove a regular file or symlink.  Refuses to remove directories.
    bool unlink(const std::string& path, const std::string& cwd);

    // ── Directory operations ─────────────────────────────────────────────────

    // Create a directory.  With parents=true, creates intermediate dirs (mkdir -p).
    bool mkdir(const std::string& path, const std::string& cwd,
               bool parents = false, uint32_t mode = 0755);

    // Remove a directory.  With recursive=true, deletes the whole subtree (rm -r).
    // Without recursive, fails if the directory is non-empty.
    bool rmdir(const std::string& path, const std::string& cwd,
               bool recursive = false);

    // List the contents of a directory.  Returns inode pointers sorted by name
    // (map order).  Does not include "." or "..".
    std::vector<Inode*> readdir(const std::string& path, const std::string& cwd);

    // ── Symlink operations ───────────────────────────────────────────────────

    // Create a symlink at `link_path` pointing to `target`.
    // `target` is stored verbatim and is NOT validated at creation time.
    bool symlink(const std::string& target, const std::string& link_path,
                 const std::string& cwd);

    // Return the raw target string of a symlink (does not follow it).
    // Sets last_error() with EINVAL if the path is not a symlink.
    std::optional<std::string> readlink(const std::string& path,
                                        const std::string& cwd);

    // ── Universal operations ─────────────────────────────────────────────────

    // Rename / move a file, directory, or symlink.
    // If dst exists and is a directory, src is moved inside it.
    bool rename(const std::string& src, const std::string& dst,
                const std::string& cwd);

    // Copy a file or directory tree.  With recursive=false, refuses to copy dirs.
    bool copy(const std::string& src, const std::string& dst,
              const std::string& cwd, bool recursive = false);

    // stat() follows symlinks; lstat() does not follow the final component.
    std::optional<StatResult> stat(const std::string& path, const std::string& cwd);
    std::optional<StatResult> lstat(const std::string& path, const std::string& cwd);

    // ── Error ────────────────────────────────────────────────────────────────

    // Human-readable message from the last failed call.  Empty on success.
    std::string last_error() const { return _last_error; }

    // ── Serialization (localStorage persistence) ─────────────────────────────

    // Serialise the entire filesystem to a JSON string.
    // Format: { "next_ino": N, "inodes": [ { ino, type, name, mode, parent,
    //           mtime, content/target, children:[ino,...] }, ... ] }
    std::string serialize() const;

    // Rebuild the VFS from a JSON string produced by serialize().
    // Calls reset() first.  Returns false on parse errors.
    bool deserialize(const std::string& json);

private:
    VFS() = default;
    VFS(const VFS&)            = delete;
    VFS& operator=(const VFS&) = delete;

    // Allocate a new inode and insert it into _inodes.
    Inode* alloc_inode(InodeType type, const std::string& name, uint32_t mode);

    // Detach node from its parent then recursively delete the subtree.
    void free_subtree(Inode* node);
    void free_subtree_recursive(Inode* node);

    // Attach `child` into `parent`'s children map and set child->parent.
    void link_child(Inode* parent, Inode* child);

    // Detach `child` from its parent's children map (does not free).
    void unlink_child(Inode* child);

    // Internal stat helper — skips or follows final symlink depending on `follow`.
    std::optional<StatResult> stat_impl(const std::string& path,
                                        const std::string& cwd, bool follow);

    // Recursively copy a subtree from `src` into directory `dst_parent`
    // with the given name.
    bool copy_subtree(Inode* src, Inode* dst_parent, const std::string& new_name);

    // Convenience: set error and return false / nullptr
    bool        err(const std::string& msg) const;
    Inode*      err_null(const std::string& msg) const;

    // Timestamp helper — seconds since epoch
    static uint64_t now();

    std::unordered_map<uint32_t, std::unique_ptr<Inode>> _inodes;
    Inode*              _root     = nullptr;
    uint32_t            _next_ino = 1;
    mutable std::string _last_error;
};
