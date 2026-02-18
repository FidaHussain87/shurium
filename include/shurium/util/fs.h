// SHURIUM - Filesystem Utilities
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Provides filesystem utilities:
// - Path manipulation
// - File operations
// - Directory operations
// - Cross-platform compatibility

#ifndef SHURIUM_UTIL_FS_H
#define SHURIUM_UTIL_FS_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <chrono>

namespace shurium {
namespace util {
namespace fs {

// ============================================================================
// Path Type
// ============================================================================

/// Cross-platform path representation
class Path {
public:
    /// Default constructor (empty path)
    Path() = default;
    
    /// Construct from string
    Path(const std::string& path);
    Path(const char* path);
    
    /// Copy and move
    Path(const Path&) = default;
    Path(Path&&) = default;
    Path& operator=(const Path&) = default;
    Path& operator=(Path&&) = default;
    
    /// Get path as string
    const std::string& String() const { return path_; }
    const char* CStr() const { return path_.c_str(); }
    
    /// Check if path is empty
    bool Empty() const { return path_.empty(); }
    
    /// Check if path is absolute
    bool IsAbsolute() const;
    
    /// Check if path is relative
    bool IsRelative() const { return !IsAbsolute(); }
    
    /// Get parent directory
    Path Parent() const;
    
    /// Get filename (last component)
    std::string Filename() const;
    
    /// Get stem (filename without extension)
    std::string Stem() const;
    
    /// Get extension (including dot)
    std::string Extension() const;
    
    /// Append path component
    Path& Append(const Path& other);
    
    /// Append with operator/
    Path operator/(const Path& other) const;
    
    /// Replace filename
    Path& ReplaceFilename(const std::string& filename);
    
    /// Replace extension
    Path& ReplaceExtension(const std::string& ext);
    
    /// Remove filename (keep directory)
    Path& RemoveFilename();
    
    /// Make path absolute
    Path Absolute() const;
    
    /// Normalize path (resolve . and ..)
    Path Normalize() const;
    
    /// Make path relative to base
    Path RelativeTo(const Path& base) const;
    
    /// Comparison operators
    bool operator==(const Path& other) const { return path_ == other.path_; }
    bool operator!=(const Path& other) const { return path_ != other.path_; }
    bool operator<(const Path& other) const { return path_ < other.path_; }
    
    /// Implicit conversion to string
    operator std::string() const { return path_; }

private:
    std::string path_;
    
    /// Normalize separators
    static std::string NormalizeSeparators(const std::string& path);
};

// ============================================================================
// Path Constants
// ============================================================================

/// Path separator for current platform
#ifdef _WIN32
constexpr char PATH_SEPARATOR = '\\';
constexpr char PATH_SEPARATOR_ALT = '/';
#else
constexpr char PATH_SEPARATOR = '/';
constexpr char PATH_SEPARATOR_ALT = '\\';
#endif

/// Get preferred separator
inline char PreferredSeparator() { return PATH_SEPARATOR; }

// ============================================================================
// File/Directory Status
// ============================================================================

/// File type enumeration
enum class FileType {
    None,           // Not found or error
    Regular,        // Regular file
    Directory,      // Directory
    Symlink,        // Symbolic link
    Block,          // Block device
    Character,      // Character device
    Fifo,           // FIFO/pipe
    Socket,         // Socket
    Unknown         // Unknown type
};

/// File permissions
struct Permissions {
    bool ownerRead{false};
    bool ownerWrite{false};
    bool ownerExecute{false};
    bool groupRead{false};
    bool groupWrite{false};
    bool groupExecute{false};
    bool othersRead{false};
    bool othersWrite{false};
    bool othersExecute{false};
    
    /// Get as numeric mode (e.g., 0755)
    uint16_t Mode() const;
    
    /// Create from numeric mode
    static Permissions FromMode(uint16_t mode);
    
    /// Default file permissions (0644)
    static Permissions DefaultFile();
    
    /// Default directory permissions (0755)
    static Permissions DefaultDirectory();
};

/// File status information
struct FileStatus {
    FileType type{FileType::None};
    Permissions permissions;
    uint64_t size{0};
    std::chrono::system_clock::time_point modifiedTime;
    std::chrono::system_clock::time_point accessTime;
    std::chrono::system_clock::time_point createdTime;
    uint64_t hardLinkCount{0};
    
    /// Check if file exists
    bool Exists() const { return type != FileType::None; }
    
    /// Check if regular file
    bool IsFile() const { return type == FileType::Regular; }
    
    /// Check if directory
    bool IsDirectory() const { return type == FileType::Directory; }
    
    /// Check if symbolic link
    bool IsSymlink() const { return type == FileType::Symlink; }
};

// ============================================================================
// File Status Functions
// ============================================================================

/// Get file status
FileStatus Status(const Path& path);

/// Get file status (don't follow symlinks)
FileStatus SymlinkStatus(const Path& path);

/// Check if path exists
bool Exists(const Path& path);

/// Check if path is a regular file
bool IsFile(const Path& path);

/// Check if path is a directory
bool IsDirectory(const Path& path);

/// Check if path is a symbolic link
bool IsSymlink(const Path& path);

/// Check if path is empty (file size 0 or empty directory)
bool IsEmpty(const Path& path);

/// Get file size
uint64_t FileSize(const Path& path);

/// Get last modification time
std::chrono::system_clock::time_point LastWriteTime(const Path& path);

/// Set last modification time
void SetLastWriteTime(const Path& path, std::chrono::system_clock::time_point time);

// ============================================================================
// File Operations
// ============================================================================

/// Copy file
/// @param from Source path
/// @param to Destination path
/// @param overwrite Overwrite if exists
/// @return true on success
bool CopyFile(const Path& from, const Path& to, bool overwrite = false);

/// Copy directory recursively
bool CopyDirectory(const Path& from, const Path& to, bool overwrite = false);

/// Move/rename file or directory
bool Rename(const Path& from, const Path& to);

/// Remove file
bool RemoveFile(const Path& path);

/// Remove directory (must be empty)
bool RemoveDirectory(const Path& path);

/// Remove directory recursively
bool RemoveAll(const Path& path);

/// Create directory
bool CreateDirectory(const Path& path);

/// Create directories (including parents)
bool CreateDirectories(const Path& path);

/// Create symbolic link
bool CreateSymlink(const Path& target, const Path& link);

/// Create hard link
bool CreateHardLink(const Path& target, const Path& link);

/// Read symbolic link target
Path ReadSymlink(const Path& path);

/// Resize file
bool ResizeFile(const Path& path, uint64_t newSize);

// ============================================================================
// Path Queries
// ============================================================================

/// Get current working directory
Path CurrentPath();

/// Set current working directory
bool SetCurrentPath(const Path& path);

/// Get temporary directory
Path TempDirectoryPath();

/// Get home directory
Path HomeDirectory();

/// Get absolute path
Path AbsolutePath(const Path& path);

/// Get canonical path (resolve symlinks)
Path CanonicalPath(const Path& path);

/// Get relative path
Path RelativePath(const Path& path, const Path& base);

/// Check if path is equivalent (same file)
bool Equivalent(const Path& p1, const Path& p2);

// ============================================================================
// Directory Iteration
// ============================================================================

/// Directory entry
struct DirectoryEntry {
    Path path;
    FileType type{FileType::Unknown};
    uint64_t size{0};
    
    DirectoryEntry() = default;
    explicit DirectoryEntry(const Path& p) : path(p) {}
};

/// List directory contents (non-recursive)
std::vector<DirectoryEntry> ListDirectory(const Path& path);

/// List directory contents with filter
std::vector<DirectoryEntry> ListDirectory(const Path& path,
                                           std::function<bool(const DirectoryEntry&)> filter);

/// Iterate directory recursively
void WalkDirectory(const Path& path,
                   std::function<void(const DirectoryEntry&)> callback);

/// Iterate directory recursively with filter
void WalkDirectory(const Path& path,
                   std::function<bool(const DirectoryEntry&)> filter,
                   std::function<void(const DirectoryEntry&)> callback);

/// Find files matching pattern (glob)
std::vector<Path> Glob(const Path& pattern);

/// Find files matching pattern recursively
std::vector<Path> GlobRecursive(const Path& directory, const std::string& pattern);

// ============================================================================
// File Content Operations
// ============================================================================

/// Read entire file as string
std::string ReadFile(const Path& path);

/// Read entire file as bytes
std::vector<uint8_t> ReadFileBytes(const Path& path);

/// Write string to file
bool WriteFile(const Path& path, const std::string& content);

/// Write bytes to file
bool WriteFile(const Path& path, const std::vector<uint8_t>& content);

/// Write bytes to file
bool WriteFile(const Path& path, const uint8_t* data, size_t size);

/// Write bytes to file with restrictive permissions (0600)
/// Use this for sensitive data like wallet files, keys, etc.
/// On Windows, sets file attributes to prevent other users from reading
bool SecureWriteFile(const Path& path, const uint8_t* data, size_t size);

/// Write bytes to file with restrictive permissions (0600)
bool SecureWriteFile(const Path& path, const std::vector<uint8_t>& content);

/// Set restrictive permissions on a file (0600 on Unix, restrictive ACL on Windows)
/// Returns true on success
bool SetSecureFilePermissions(const Path& path);

/// Append string to file
bool AppendFile(const Path& path, const std::string& content);

/// Append bytes to file
bool AppendFile(const Path& path, const std::vector<uint8_t>& content);

// ============================================================================
// Temporary Files
// ============================================================================

/// Create temporary file
/// @return Path to created file (empty on failure)
Path CreateTempFile(const std::string& prefix = "nexus_");

/// Create temporary directory
/// @return Path to created directory (empty on failure)
Path CreateTempDirectory(const std::string& prefix = "nexus_");

/// RAII temporary file (deleted on destruction)
class TempFile {
public:
    TempFile();
    explicit TempFile(const std::string& prefix);
    ~TempFile();
    
    // Non-copyable but movable
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
    TempFile(TempFile&& other) noexcept;
    TempFile& operator=(TempFile&& other) noexcept;
    
    /// Get path
    const Path& GetPath() const { return path_; }
    
    /// Release ownership (won't be deleted)
    Path Release();
    
    /// Check if valid
    bool IsValid() const { return !path_.Empty(); }

private:
    Path path_;
};

/// RAII temporary directory (deleted on destruction)
class TempDirectory {
public:
    TempDirectory();
    explicit TempDirectory(const std::string& prefix);
    ~TempDirectory();
    
    // Non-copyable but movable
    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;
    TempDirectory(TempDirectory&& other) noexcept;
    TempDirectory& operator=(TempDirectory&& other) noexcept;
    
    /// Get path
    const Path& GetPath() const { return path_; }
    
    /// Release ownership
    Path Release();
    
    /// Check if valid
    bool IsValid() const { return !path_.Empty(); }

private:
    Path path_;
};

// ============================================================================
// Disk Space
// ============================================================================

/// Disk space information
struct SpaceInfo {
    uint64_t capacity{0};       // Total capacity in bytes
    uint64_t free{0};           // Free space in bytes
    uint64_t available{0};      // Available space for non-privileged users
    
    /// Get used space
    uint64_t Used() const { return capacity - free; }
    
    /// Get usage percentage
    double UsagePercent() const {
        return capacity > 0 ? (100.0 * Used() / capacity) : 0.0;
    }
};

/// Get disk space information
SpaceInfo DiskSpace(const Path& path);

// ============================================================================
// File Locking
// ============================================================================

/// File lock type
enum class LockType {
    Shared,     // Read lock (multiple allowed)
    Exclusive   // Write lock (only one allowed)
};

/// Lock file (RAII)
class FileLock {
public:
    FileLock();
    explicit FileLock(const Path& path, LockType type = LockType::Exclusive);
    ~FileLock();
    
    // Non-copyable but movable
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;
    FileLock(FileLock&& other) noexcept;
    FileLock& operator=(FileLock&& other) noexcept;
    
    /// Acquire lock
    bool Lock(const Path& path, LockType type = LockType::Exclusive);
    
    /// Try to acquire lock (non-blocking)
    bool TryLock(const Path& path, LockType type = LockType::Exclusive);
    
    /// Release lock
    void Unlock();
    
    /// Check if locked
    bool IsLocked() const { return locked_; }
    
    /// Get locked path
    const Path& GetPath() const { return path_; }

private:
    Path path_;
    int fd_{-1};
    bool locked_{false};
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Get file extension (lowercase, without dot)
std::string GetExtension(const Path& path);

/// Check if file has extension
bool HasExtension(const Path& path, const std::string& ext);

/// Join paths
Path JoinPaths(const Path& p1, const Path& p2);

/// Join multiple paths
template<typename... Paths>
Path JoinPaths(const Path& first, const Path& second, const Paths&... rest) {
    return JoinPaths(JoinPaths(first, second), rest...);
}

/// Expand ~ in path to home directory
Path ExpandUser(const Path& path);

/// Expand environment variables in path
Path ExpandVars(const Path& path);

/// Make path safe for use in filenames
std::string SanitizeFilename(const std::string& name);

/// Get unique filename (add number if exists)
Path UniqueFilename(const Path& path);

/// Calculate file checksum (SHA256)
std::string FileChecksum(const Path& path);

} // namespace fs
} // namespace util
} // namespace shurium

#endif // SHURIUM_UTIL_FS_H
