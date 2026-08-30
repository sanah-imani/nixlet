#pragma once
#include <string>
#include <map>
#include <cstdint>
#include <memory>

// ---------------------------------------------------------------------------
// InodeType
// ---------------------------------------------------------------------------
enum class InodeType { File, Directory, Symlink };

// ---------------------------------------------------------------------------
// Inode
//
// Owned exclusively by VFS via unique_ptr in a flat ino→Inode map.
// Raw pointers are used for parent/children — the VFS is the sole owner.
//
// For Symlink nodes, `content` holds the link target string (e.g. "../foo").
// For File nodes,    `content` holds the file data.
// For Directory nodes, `content` is always empty; children are in `children`.
// ---------------------------------------------------------------------------
struct Inode {
    uint32_t    ino  = 0;
    InodeType   type = InodeType::File;
    std::string name;       // basename only — never a full path
    std::string content;    // file data OR symlink target
    uint32_t    mode = 0;
    uint64_t    atime = 0;  // seconds since epoch
    uint64_t    mtime = 0;
    uint64_t    ctime = 0;

    // Tree links — raw pointers, VFS owns the memory
    Inode*                          parent   = nullptr;
    std::map<std::string, Inode*>   children; // populated for Directory only

    // ── Convenience predicates ──────────────────────────────────────────────
    bool is_file()    const noexcept { return type == InodeType::File; }
    bool is_dir()     const noexcept { return type == InodeType::Directory; }
    bool is_symlink() const noexcept { return type == InodeType::Symlink; }

    // Size: byte count of content for files; 0 for dirs and symlinks
    uint64_t size() const noexcept { return is_file() ? content.size() : 0; }

    // Symlink target (only meaningful when is_symlink() == true)
    const std::string& link_target() const noexcept { return content; }
};
