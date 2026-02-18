// SHURIUM - Filesystem Utilities Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/util/fs.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define PATH_MAX MAX_PATH
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <pwd.h>
#include <utime.h>
#include <climits>
#endif

namespace shurium {
namespace util {
namespace fs {

// ============================================================================
// Path Implementation
// ============================================================================

Path::Path(const std::string& path) : path_(NormalizeSeparators(path)) {}

Path::Path(const char* path) : path_(NormalizeSeparators(path ? path : "")) {}

std::string Path::NormalizeSeparators(const std::string& path) {
    std::string result = path;
    // Convert all separators to preferred separator
    for (char& c : result) {
        if (c == PATH_SEPARATOR_ALT) {
            c = PATH_SEPARATOR;
        }
    }
    return result;
}

bool Path::IsAbsolute() const {
#ifdef _WIN32
    // Windows: starts with drive letter or UNC path
    if (path_.length() >= 2) {
        if (path_[1] == ':') return true;  // C:\ style
        if (path_[0] == PATH_SEPARATOR && path_[1] == PATH_SEPARATOR) return true; // UNC
    }
    return false;
#else
    return !path_.empty() && path_[0] == PATH_SEPARATOR;
#endif
}

Path Path::Parent() const {
    if (path_.empty()) return Path();
    
    size_t pos = path_.find_last_of(PATH_SEPARATOR);
    if (pos == std::string::npos) {
        return Path();
    }
    if (pos == 0) {
        return Path(std::string(1, PATH_SEPARATOR));
    }
    return Path(path_.substr(0, pos));
}

std::string Path::Filename() const {
    if (path_.empty()) return "";
    
    size_t pos = path_.find_last_of(PATH_SEPARATOR);
    if (pos == std::string::npos) {
        return path_;
    }
    return path_.substr(pos + 1);
}

std::string Path::Stem() const {
    std::string name = Filename();
    if (name.empty() || name == "." || name == "..") {
        return name;
    }
    
    size_t pos = name.find_last_of('.');
    if (pos == std::string::npos || pos == 0) {
        return name;
    }
    return name.substr(0, pos);
}

std::string Path::Extension() const {
    std::string name = Filename();
    if (name.empty() || name == "." || name == "..") {
        return "";
    }
    
    size_t pos = name.find_last_of('.');
    if (pos == std::string::npos || pos == 0) {
        return "";
    }
    return name.substr(pos);
}

Path& Path::Append(const Path& other) {
    if (other.path_.empty()) {
        return *this;
    }
    if (path_.empty()) {
        path_ = other.path_;
        return *this;
    }
    
    // If other is absolute, replace
    if (other.IsAbsolute()) {
        path_ = other.path_;
        return *this;
    }
    
    // Append with separator
    if (path_.back() != PATH_SEPARATOR) {
        path_ += PATH_SEPARATOR;
    }
    path_ += other.path_;
    return *this;
}

Path Path::operator/(const Path& other) const {
    Path result(*this);
    result.Append(other);
    return result;
}

Path& Path::ReplaceFilename(const std::string& filename) {
    *this = Parent() / Path(filename);
    return *this;
}

Path& Path::ReplaceExtension(const std::string& ext) {
    std::string stem = Stem();
    Path parent = Parent();
    std::string newExt = ext;
    
    // Ensure extension starts with dot
    if (!newExt.empty() && newExt[0] != '.') {
        newExt = "." + newExt;
    }
    
    if (parent.Empty()) {
        path_ = stem + newExt;
    } else {
        *this = parent / Path(stem + newExt);
    }
    return *this;
}

Path& Path::RemoveFilename() {
    *this = Parent();
    return *this;
}

Path Path::Absolute() const {
    return AbsolutePath(*this);
}

Path Path::Normalize() const {
    if (path_.empty()) return Path();
    
    std::vector<std::string> components;
    std::istringstream iss(path_);
    std::string component;
    bool isAbsolute = IsAbsolute();
    
    while (std::getline(iss, component, PATH_SEPARATOR)) {
        if (component.empty() || component == ".") {
            continue;
        }
        if (component == "..") {
            if (!components.empty() && components.back() != "..") {
                components.pop_back();
            } else if (!isAbsolute) {
                components.push_back("..");
            }
        } else {
            components.push_back(component);
        }
    }
    
    if (components.empty()) {
        return isAbsolute ? Path(std::string(1, PATH_SEPARATOR)) : Path(".");
    }
    
    std::ostringstream oss;
    if (isAbsolute) {
#ifdef _WIN32
        // Preserve drive letter
        if (path_.length() >= 2 && path_[1] == ':') {
            oss << path_.substr(0, 2);
        }
#endif
        oss << PATH_SEPARATOR;
    }
    
    for (size_t i = 0; i < components.size(); ++i) {
        if (i > 0) oss << PATH_SEPARATOR;
        oss << components[i];
    }
    
    return Path(oss.str());
}

Path Path::RelativeTo(const Path& base) const {
    return RelativePath(*this, base);
}

// ============================================================================
// Permissions Implementation
// ============================================================================

uint16_t Permissions::Mode() const {
    uint16_t mode = 0;
    if (ownerRead) mode |= 0400;
    if (ownerWrite) mode |= 0200;
    if (ownerExecute) mode |= 0100;
    if (groupRead) mode |= 0040;
    if (groupWrite) mode |= 0020;
    if (groupExecute) mode |= 0010;
    if (othersRead) mode |= 0004;
    if (othersWrite) mode |= 0002;
    if (othersExecute) mode |= 0001;
    return mode;
}

Permissions Permissions::FromMode(uint16_t mode) {
    Permissions p;
    p.ownerRead = (mode & 0400) != 0;
    p.ownerWrite = (mode & 0200) != 0;
    p.ownerExecute = (mode & 0100) != 0;
    p.groupRead = (mode & 0040) != 0;
    p.groupWrite = (mode & 0020) != 0;
    p.groupExecute = (mode & 0010) != 0;
    p.othersRead = (mode & 0004) != 0;
    p.othersWrite = (mode & 0002) != 0;
    p.othersExecute = (mode & 0001) != 0;
    return p;
}

Permissions Permissions::DefaultFile() {
    return FromMode(0644);
}

Permissions Permissions::DefaultDirectory() {
    return FromMode(0755);
}

// ============================================================================
// File Status Functions
// ============================================================================

FileStatus Status(const Path& path) {
    FileStatus status;
    
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(path.CStr(), GetFileExInfoStandard, &attrs)) {
        return status;
    }
    
    if (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        status.type = FileType::Directory;
    } else {
        status.type = FileType::Regular;
    }
    
    status.size = (static_cast<uint64_t>(attrs.nFileSizeHigh) << 32) | attrs.nFileSizeLow;
    
    // Convert FILETIME to system_clock::time_point
    auto ftToTimePoint = [](FILETIME ft) {
        ULARGE_INTEGER ull;
        ull.LowPart = ft.dwLowDateTime;
        ull.HighPart = ft.dwHighDateTime;
        // FILETIME is 100-nanosecond intervals since January 1, 1601
        // Convert to seconds since Unix epoch
        int64_t unixTime = (ull.QuadPart / 10000000ULL) - 11644473600ULL;
        return std::chrono::system_clock::from_time_t(unixTime);
    };
    
    status.modifiedTime = ftToTimePoint(attrs.ftLastWriteTime);
    status.accessTime = ftToTimePoint(attrs.ftLastAccessTime);
    status.createdTime = ftToTimePoint(attrs.ftCreationTime);
#else
    struct stat st;
    if (stat(path.CStr(), &st) != 0) {
        return status;
    }
    
    // Determine file type
    if (S_ISREG(st.st_mode)) status.type = FileType::Regular;
    else if (S_ISDIR(st.st_mode)) status.type = FileType::Directory;
    else if (S_ISLNK(st.st_mode)) status.type = FileType::Symlink;
    else if (S_ISBLK(st.st_mode)) status.type = FileType::Block;
    else if (S_ISCHR(st.st_mode)) status.type = FileType::Character;
    else if (S_ISFIFO(st.st_mode)) status.type = FileType::Fifo;
    else if (S_ISSOCK(st.st_mode)) status.type = FileType::Socket;
    else status.type = FileType::Unknown;
    
    status.permissions = Permissions::FromMode(st.st_mode & 0777);
    status.size = static_cast<uint64_t>(st.st_size);
    status.hardLinkCount = static_cast<uint64_t>(st.st_nlink);
    
    status.modifiedTime = std::chrono::system_clock::from_time_t(st.st_mtime);
    status.accessTime = std::chrono::system_clock::from_time_t(st.st_atime);
    status.createdTime = std::chrono::system_clock::from_time_t(st.st_ctime);
#endif
    
    return status;
}

FileStatus SymlinkStatus(const Path& path) {
#ifndef _WIN32
    FileStatus status;
    struct stat st;
    if (lstat(path.CStr(), &st) != 0) {
        return status;
    }
    
    if (S_ISREG(st.st_mode)) status.type = FileType::Regular;
    else if (S_ISDIR(st.st_mode)) status.type = FileType::Directory;
    else if (S_ISLNK(st.st_mode)) status.type = FileType::Symlink;
    else status.type = FileType::Unknown;
    
    status.permissions = Permissions::FromMode(st.st_mode & 0777);
    status.size = static_cast<uint64_t>(st.st_size);
    
    return status;
#else
    return Status(path);
#endif
}

bool Exists(const Path& path) {
    return Status(path).Exists();
}

bool IsFile(const Path& path) {
    return Status(path).IsFile();
}

bool IsDirectory(const Path& path) {
    return Status(path).IsDirectory();
}

bool IsSymlink(const Path& path) {
    return SymlinkStatus(path).IsSymlink();
}

bool IsEmpty(const Path& path) {
    auto status = Status(path);
    if (status.IsFile()) {
        return status.size == 0;
    }
    if (status.IsDirectory()) {
        auto entries = ListDirectory(path);
        return entries.empty();
    }
    return false;
}

uint64_t FileSize(const Path& path) {
    return Status(path).size;
}

std::chrono::system_clock::time_point LastWriteTime(const Path& path) {
    return Status(path).modifiedTime;
}

void SetLastWriteTime(const Path& path, std::chrono::system_clock::time_point time) {
#ifndef _WIN32
    struct utimbuf times;
    times.actime = std::chrono::system_clock::to_time_t(time);
    times.modtime = times.actime;
    utime(path.CStr(), &times);
#endif
}

// ============================================================================
// File Operations
// ============================================================================

bool CopyFile(const Path& from, const Path& to, bool overwrite) {
    if (!overwrite && Exists(to)) {
        return false;
    }
    
    std::ifstream src(from.String(), std::ios::binary);
    if (!src) return false;
    
    std::ofstream dst(to.String(), std::ios::binary);
    if (!dst) return false;
    
    dst << src.rdbuf();
    return src.good() && dst.good();
}

bool CopyDirectory(const Path& from, const Path& to, bool overwrite) {
    if (!IsDirectory(from)) {
        return false;
    }
    
    if (!Exists(to)) {
        if (!CreateDirectories(to)) {
            return false;
        }
    }
    
    auto entries = ListDirectory(from);
    for (const auto& entry : entries) {
        Path destPath = to / entry.path.Filename();
        
        if (entry.type == FileType::Directory) {
            if (!CopyDirectory(entry.path, destPath, overwrite)) {
                return false;
            }
        } else {
            if (!CopyFile(entry.path, destPath, overwrite)) {
                return false;
            }
        }
    }
    
    return true;
}

bool Rename(const Path& from, const Path& to) {
    return std::rename(from.CStr(), to.CStr()) == 0;
}

bool RemoveFile(const Path& path) {
    return std::remove(path.CStr()) == 0;
}

bool RemoveDirectory(const Path& path) {
#ifdef _WIN32
    return RemoveDirectoryA(path.CStr()) != 0;
#else
    return rmdir(path.CStr()) == 0;
#endif
}

bool RemoveAll(const Path& path) {
    if (!Exists(path)) {
        return true;
    }
    
    if (IsDirectory(path)) {
        auto entries = ListDirectory(path);
        for (const auto& entry : entries) {
            if (!RemoveAll(entry.path)) {
                return false;
            }
        }
        return RemoveDirectory(path);
    }
    
    return RemoveFile(path);
}

bool CreateDirectory(const Path& path) {
#ifdef _WIN32
    return CreateDirectoryA(path.CStr(), nullptr) != 0;
#else
    return mkdir(path.CStr(), 0755) == 0;
#endif
}

bool CreateDirectories(const Path& path) {
    if (path.Empty()) return false;
    if (Exists(path)) return IsDirectory(path);
    
    Path parent = path.Parent();
    if (!parent.Empty() && !Exists(parent)) {
        if (!CreateDirectories(parent)) {
            return false;
        }
    }
    
    return CreateDirectory(path);
}

bool CreateSymlink(const Path& target, const Path& link) {
#ifdef _WIN32
    DWORD flags = IsDirectory(target) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
    return CreateSymbolicLinkA(link.CStr(), target.CStr(), flags) != 0;
#else
    return symlink(target.CStr(), link.CStr()) == 0;
#endif
}

bool CreateHardLink(const Path& target, const Path& link) {
#ifdef _WIN32
    return CreateHardLinkA(link.CStr(), target.CStr(), nullptr) != 0;
#else
    return ::link(target.CStr(), link.CStr()) == 0;
#endif
}

Path ReadSymlink(const Path& path) {
#ifdef _WIN32
    return Path();  // Not easily supported on Windows
#else
    char buf[PATH_MAX];
    ssize_t len = readlink(path.CStr(), buf, sizeof(buf) - 1);
    if (len < 0) return Path();
    buf[len] = '\0';
    return Path(buf);
#endif
}

bool ResizeFile(const Path& path, uint64_t newSize) {
#ifdef _WIN32
    HANDLE h = CreateFileA(path.CStr(), GENERIC_WRITE, 0, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    
    LARGE_INTEGER li;
    li.QuadPart = newSize;
    bool result = SetFilePointerEx(h, li, nullptr, FILE_BEGIN) &&
                  SetEndOfFile(h);
    CloseHandle(h);
    return result;
#else
    return truncate(path.CStr(), static_cast<off_t>(newSize)) == 0;
#endif
}

// ============================================================================
// Path Queries
// ============================================================================

Path CurrentPath() {
#ifdef _WIN32
    char buf[MAX_PATH];
    if (_getcwd(buf, MAX_PATH) == nullptr) return Path();
    return Path(buf);
#else
    char buf[PATH_MAX];
    if (getcwd(buf, PATH_MAX) == nullptr) return Path();
    return Path(buf);
#endif
}

bool SetCurrentPath(const Path& path) {
#ifdef _WIN32
    return _chdir(path.CStr()) == 0;
#else
    return chdir(path.CStr()) == 0;
#endif
}

Path TempDirectoryPath() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD len = GetTempPathA(MAX_PATH, buf);
    if (len == 0 || len > MAX_PATH) return Path();
    return Path(buf);
#else
    const char* tmpdir = getenv("TMPDIR");
    if (tmpdir) return Path(tmpdir);
    return Path("/tmp");
#endif
}

Path HomeDirectory() {
#ifdef _WIN32
    const char* home = getenv("USERPROFILE");
    if (home) return Path(home);
    
    const char* drive = getenv("HOMEDRIVE");
    const char* path = getenv("HOMEPATH");
    if (drive && path) return Path(std::string(drive) + path);
    
    return Path();
#else
    const char* home = getenv("HOME");
    if (home) return Path(home);
    
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) return Path(pw->pw_dir);
    
    return Path();
#endif
}

Path AbsolutePath(const Path& path) {
    if (path.IsAbsolute()) return path;
    return CurrentPath() / path;
}

Path CanonicalPath(const Path& path) {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD len = GetFullPathNameA(path.CStr(), MAX_PATH, buf, nullptr);
    if (len == 0 || len > MAX_PATH) return Path();
    return Path(buf);
#else
    char buf[PATH_MAX];
    if (realpath(path.CStr(), buf) == nullptr) return Path();
    return Path(buf);
#endif
}

Path RelativePath(const Path& path, const Path& base) {
    Path absPath = AbsolutePath(path).Normalize();
    Path absBase = AbsolutePath(base).Normalize();
    
    // Simple implementation: count common prefix
    std::string pathStr = absPath.String();
    std::string baseStr = absBase.String();
    
    // Find common prefix length
    size_t commonLen = 0;
    size_t lastSep = 0;
    for (size_t i = 0; i < pathStr.length() && i < baseStr.length(); ++i) {
        if (pathStr[i] != baseStr[i]) break;
        if (pathStr[i] == PATH_SEPARATOR) lastSep = i;
        commonLen = i + 1;
    }
    
    if (lastSep > 0) commonLen = lastSep + 1;
    
    // Count directories to go up from base
    std::string remaining = baseStr.substr(commonLen);
    int upCount = 0;
    for (char c : remaining) {
        if (c == PATH_SEPARATOR) upCount++;
    }
    if (!remaining.empty() && remaining.back() != PATH_SEPARATOR) upCount++;
    
    // Build relative path
    std::ostringstream oss;
    for (int i = 0; i < upCount; ++i) {
        if (i > 0) oss << PATH_SEPARATOR;
        oss << "..";
    }
    
    std::string pathRemaining = pathStr.substr(commonLen);
    if (!pathRemaining.empty()) {
        if (upCount > 0) oss << PATH_SEPARATOR;
        oss << pathRemaining;
    }
    
    std::string result = oss.str();
    return result.empty() ? Path(".") : Path(result);
}

bool Equivalent(const Path& p1, const Path& p2) {
    return CanonicalPath(p1) == CanonicalPath(p2);
}

// ============================================================================
// Directory Iteration
// ============================================================================

std::vector<DirectoryEntry> ListDirectory(const Path& path) {
    std::vector<DirectoryEntry> entries;
    
#ifdef _WIN32
    WIN32_FIND_DATAA findData;
    std::string pattern = path.String() + "\\*";
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) return entries;
    
    do {
        std::string name = findData.cFileName;
        if (name == "." || name == "..") continue;
        
        DirectoryEntry entry;
        entry.path = path / name;
        entry.type = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                     ? FileType::Directory : FileType::Regular;
        entry.size = (static_cast<uint64_t>(findData.nFileSizeHigh) << 32) |
                     findData.nFileSizeLow;
        entries.push_back(entry);
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
#else
    DIR* dir = opendir(path.CStr());
    if (!dir) return entries;
    
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;
        
        DirectoryEntry entry;
        entry.path = path / name;
        
        // Get full status for type and size
        auto status = Status(entry.path);
        entry.type = status.type;
        entry.size = status.size;
        
        entries.push_back(entry);
    }
    
    closedir(dir);
#endif
    
    return entries;
}

std::vector<DirectoryEntry> ListDirectory(const Path& path,
                                          std::function<bool(const DirectoryEntry&)> filter) {
    auto entries = ListDirectory(path);
    std::vector<DirectoryEntry> filtered;
    
    for (const auto& entry : entries) {
        if (filter(entry)) {
            filtered.push_back(entry);
        }
    }
    
    return filtered;
}

void WalkDirectory(const Path& path,
                   std::function<void(const DirectoryEntry&)> callback) {
    auto entries = ListDirectory(path);
    
    for (const auto& entry : entries) {
        callback(entry);
        
        if (entry.type == FileType::Directory) {
            WalkDirectory(entry.path, callback);
        }
    }
}

void WalkDirectory(const Path& path,
                   std::function<bool(const DirectoryEntry&)> filter,
                   std::function<void(const DirectoryEntry&)> callback) {
    auto entries = ListDirectory(path);
    
    for (const auto& entry : entries) {
        if (filter(entry)) {
            callback(entry);
        }
        
        if (entry.type == FileType::Directory) {
            WalkDirectory(entry.path, filter, callback);
        }
    }
}

std::vector<Path> Glob(const Path& pattern) {
    std::vector<Path> results;
    
    // Simple implementation: extract directory and pattern
    Path dir = pattern.Parent();
    std::string pat = pattern.Filename();
    
    if (dir.Empty()) dir = Path(".");
    if (!IsDirectory(dir)) return results;
    
    auto entries = ListDirectory(dir);
    
    for (const auto& entry : entries) {
        std::string name = entry.path.Filename();
        
        // Simple wildcard matching
        bool match = true;
        size_t pi = 0, ni = 0;
        
        while (pi < pat.length() && ni < name.length()) {
            if (pat[pi] == '*') {
                // Skip consecutive stars
                while (pi < pat.length() && pat[pi] == '*') pi++;
                if (pi == pat.length()) {
                    ni = name.length();
                    break;
                }
                // Find next matching char
                while (ni < name.length() && name[ni] != pat[pi]) ni++;
            } else if (pat[pi] == '?' || pat[pi] == name[ni]) {
                pi++;
                ni++;
            } else {
                match = false;
                break;
            }
        }
        
        // Handle trailing stars
        while (pi < pat.length() && pat[pi] == '*') pi++;
        
        if (match && pi == pat.length() && ni == name.length()) {
            results.push_back(entry.path);
        }
    }
    
    return results;
}

std::vector<Path> GlobRecursive(const Path& directory, const std::string& pattern) {
    std::vector<Path> results;
    
    WalkDirectory(directory, [&](const DirectoryEntry& entry) {
        std::string name = entry.path.Filename();
        
        // Simple pattern matching
        bool match = true;
        size_t pi = 0, ni = 0;
        
        while (pi < pattern.length() && ni < name.length()) {
            if (pattern[pi] == '*') {
                while (pi < pattern.length() && pattern[pi] == '*') pi++;
                if (pi == pattern.length()) {
                    ni = name.length();
                    break;
                }
                while (ni < name.length() && name[ni] != pattern[pi]) ni++;
            } else if (pattern[pi] == '?' || pattern[pi] == name[ni]) {
                pi++;
                ni++;
            } else {
                match = false;
                break;
            }
        }
        
        while (pi < pattern.length() && pattern[pi] == '*') pi++;
        
        if (match && pi == pattern.length() && ni == name.length()) {
            results.push_back(entry.path);
        }
    });
    
    return results;
}

// ============================================================================
// File Content Operations
// ============================================================================

std::string ReadFile(const Path& path) {
    std::ifstream file(path.String(), std::ios::binary);
    if (!file) return "";
    
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

std::vector<uint8_t> ReadFileBytes(const Path& path) {
    std::ifstream file(path.String(), std::ios::binary | std::ios::ate);
    if (!file) return {};
    
    auto size = file.tellg();
    file.seekg(0);
    
    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);
    
    return data;
}

bool WriteFile(const Path& path, const std::string& content) {
    std::ofstream file(path.String(), std::ios::binary);
    if (!file) return false;
    
    file.write(content.data(), content.size());
    return file.good();
}

bool WriteFile(const Path& path, const std::vector<uint8_t>& content) {
    return WriteFile(path, content.data(), content.size());
}

bool WriteFile(const Path& path, const uint8_t* data, size_t size) {
    std::ofstream file(path.String(), std::ios::binary);
    if (!file) return false;
    
    file.write(reinterpret_cast<const char*>(data), size);
    return file.good();
}

bool SetSecureFilePermissions(const Path& path) {
#ifdef _WIN32
    // On Windows, make the file readable only by the owner
    // This is a simplified version - for full security, use SetFileSecurity()
    return SetFileAttributesA(path.CStr(), FILE_ATTRIBUTE_NORMAL) != 0;
#else
    // On Unix, set permissions to 0600 (owner read/write only)
    return chmod(path.CStr(), S_IRUSR | S_IWUSR) == 0;
#endif
}

bool SecureWriteFile(const Path& path, const uint8_t* data, size_t size) {
#ifdef _WIN32
    // Windows: Create file with restrictive security descriptor
    // For simplicity, create normally then restrict
    std::ofstream file(path.String(), std::ios::binary);
    if (!file) return false;
    
    file.write(reinterpret_cast<const char*>(data), size);
    if (!file.good()) return false;
    file.close();
    
    return SetSecureFilePermissions(path);
#else
    // Unix: Create file with restrictive permissions from the start
    // Use open() with O_CREAT and specific mode, then write
    int fd = open(path.CStr(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) return false;
    
    size_t written = 0;
    while (written < size) {
        ssize_t result = write(fd, data + written, size - written);
        if (result < 0) {
            close(fd);
            return false;
        }
        written += static_cast<size_t>(result);
    }
    
    // Ensure data is flushed to disk
    fsync(fd);
    close(fd);
    
    return true;
#endif
}

bool SecureWriteFile(const Path& path, const std::vector<uint8_t>& content) {
    return SecureWriteFile(path, content.data(), content.size());
}

bool AppendFile(const Path& path, const std::string& content) {
    std::ofstream file(path.String(), std::ios::binary | std::ios::app);
    if (!file) return false;
    
    file.write(content.data(), content.size());
    return file.good();
}

bool AppendFile(const Path& path, const std::vector<uint8_t>& content) {
    std::ofstream file(path.String(), std::ios::binary | std::ios::app);
    if (!file) return false;
    
    file.write(reinterpret_cast<const char*>(content.data()), content.size());
    return file.good();
}

// ============================================================================
// Temporary Files
// ============================================================================

namespace {
    std::string GenerateRandomString(size_t length) {
        static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<size_t> dist(0, sizeof(chars) - 2);
        
        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result += chars[dist(gen)];
        }
        return result;
    }
}

Path CreateTempFile(const std::string& prefix) {
    Path tempDir = TempDirectoryPath();
    
    for (int i = 0; i < 100; ++i) {
        std::string name = prefix + GenerateRandomString(10);
        Path path = tempDir / name;
        
        if (!Exists(path)) {
            // Create empty file
            std::ofstream file(path.String());
            if (file) {
                return path;
            }
        }
    }
    
    return Path();
}

Path CreateTempDirectory(const std::string& prefix) {
    Path tempDir = TempDirectoryPath();
    
    for (int i = 0; i < 100; ++i) {
        std::string name = prefix + GenerateRandomString(10);
        Path path = tempDir / name;
        
        if (!Exists(path)) {
            if (CreateDirectory(path)) {
                return path;
            }
        }
    }
    
    return Path();
}

TempFile::TempFile() : path_(CreateTempFile()) {}

TempFile::TempFile(const std::string& prefix) : path_(CreateTempFile(prefix)) {}

TempFile::~TempFile() {
    if (!path_.Empty()) {
        RemoveFile(path_);
    }
}

TempFile::TempFile(TempFile&& other) noexcept : path_(std::move(other.path_)) {
    other.path_ = Path();
}

TempFile& TempFile::operator=(TempFile&& other) noexcept {
    if (this != &other) {
        if (!path_.Empty()) {
            RemoveFile(path_);
        }
        path_ = std::move(other.path_);
        other.path_ = Path();
    }
    return *this;
}

Path TempFile::Release() {
    Path result = path_;
    path_ = Path();
    return result;
}

TempDirectory::TempDirectory() : path_(CreateTempDirectory()) {}

TempDirectory::TempDirectory(const std::string& prefix) 
    : path_(CreateTempDirectory(prefix)) {}

TempDirectory::~TempDirectory() {
    if (!path_.Empty()) {
        RemoveAll(path_);
    }
}

TempDirectory::TempDirectory(TempDirectory&& other) noexcept 
    : path_(std::move(other.path_)) {
    other.path_ = Path();
}

TempDirectory& TempDirectory::operator=(TempDirectory&& other) noexcept {
    if (this != &other) {
        if (!path_.Empty()) {
            RemoveAll(path_);
        }
        path_ = std::move(other.path_);
        other.path_ = Path();
    }
    return *this;
}

Path TempDirectory::Release() {
    Path result = path_;
    path_ = Path();
    return result;
}

// ============================================================================
// Disk Space
// ============================================================================

SpaceInfo DiskSpace(const Path& path) {
    SpaceInfo info;
    
#ifdef _WIN32
    ULARGE_INTEGER freeBytesAvailable, totalBytes, freeBytes;
    if (GetDiskFreeSpaceExA(path.CStr(), &freeBytesAvailable, 
                            &totalBytes, &freeBytes)) {
        info.capacity = totalBytes.QuadPart;
        info.free = freeBytes.QuadPart;
        info.available = freeBytesAvailable.QuadPart;
    }
#else
    struct statvfs st;
    if (statvfs(path.CStr(), &st) == 0) {
        info.capacity = static_cast<uint64_t>(st.f_blocks) * st.f_frsize;
        info.free = static_cast<uint64_t>(st.f_bfree) * st.f_frsize;
        info.available = static_cast<uint64_t>(st.f_bavail) * st.f_frsize;
    }
#endif
    
    return info;
}

// ============================================================================
// File Locking
// ============================================================================

FileLock::FileLock() : fd_(-1), locked_(false) {}

FileLock::FileLock(const Path& path, LockType type) : fd_(-1), locked_(false) {
    Lock(path, type);
}

FileLock::~FileLock() {
    Unlock();
}

FileLock::FileLock(FileLock&& other) noexcept
    : path_(std::move(other.path_))
    , fd_(other.fd_)
    , locked_(other.locked_) {
    other.fd_ = -1;
    other.locked_ = false;
}

FileLock& FileLock::operator=(FileLock&& other) noexcept {
    if (this != &other) {
        Unlock();
        path_ = std::move(other.path_);
        fd_ = other.fd_;
        locked_ = other.locked_;
        other.fd_ = -1;
        other.locked_ = false;
    }
    return *this;
}

bool FileLock::Lock(const Path& path, LockType type) {
    if (locked_) {
        Unlock();
    }
    
#ifdef _WIN32
    // Windows implementation would use LockFileEx
    return false;
#else
    fd_ = open(path.CStr(), O_RDWR | O_CREAT, 0644);
    if (fd_ < 0) return false;
    
    int operation = (type == LockType::Shared) ? LOCK_SH : LOCK_EX;
    if (flock(fd_, operation) == 0) {
        path_ = path;
        locked_ = true;
        return true;
    }
    
    close(fd_);
    fd_ = -1;
    return false;
#endif
}

bool FileLock::TryLock(const Path& path, LockType type) {
    if (locked_) {
        Unlock();
    }
    
#ifdef _WIN32
    return false;
#else
    fd_ = open(path.CStr(), O_RDWR | O_CREAT, 0644);
    if (fd_ < 0) return false;
    
    int operation = (type == LockType::Shared) ? LOCK_SH : LOCK_EX;
    operation |= LOCK_NB;
    
    if (flock(fd_, operation) == 0) {
        path_ = path;
        locked_ = true;
        return true;
    }
    
    close(fd_);
    fd_ = -1;
    return false;
#endif
}

void FileLock::Unlock() {
    if (!locked_) return;
    
#ifndef _WIN32
    if (fd_ >= 0) {
        flock(fd_, LOCK_UN);
        close(fd_);
        fd_ = -1;
    }
#endif
    
    locked_ = false;
    path_ = Path();
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string GetExtension(const Path& path) {
    std::string ext = path.Extension();
    if (ext.empty()) return "";
    
    // Remove leading dot and convert to lowercase
    ext = ext.substr(1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

bool HasExtension(const Path& path, const std::string& ext) {
    std::string pathExt = GetExtension(path);
    std::string checkExt = ext;
    
    // Remove leading dot from check extension
    if (!checkExt.empty() && checkExt[0] == '.') {
        checkExt = checkExt.substr(1);
    }
    
    std::transform(checkExt.begin(), checkExt.end(), checkExt.begin(), ::tolower);
    return pathExt == checkExt;
}

Path JoinPaths(const Path& p1, const Path& p2) {
    return p1 / p2;
}

Path ExpandUser(const Path& path) {
    std::string str = path.String();
    if (str.empty() || str[0] != '~') {
        return path;
    }
    
    Path home = HomeDirectory();
    if (home.Empty()) return path;
    
    if (str.length() == 1) {
        return home;
    }
    
    if (str[1] == PATH_SEPARATOR) {
        return home / str.substr(2);
    }
    
    return path;
}

Path ExpandVars(const Path& path) {
    std::string str = path.String();
    std::string result;
    result.reserve(str.length());
    
    size_t i = 0;
    while (i < str.length()) {
        if (str[i] == '$') {
            // Find variable name
            size_t start = i + 1;
            size_t end = start;
            
            if (start < str.length() && str[start] == '{') {
                // ${VAR} format
                start++;
                end = str.find('}', start);
                if (end != std::string::npos) {
                    std::string varName = str.substr(start, end - start);
                    const char* value = getenv(varName.c_str());
                    if (value) {
                        result += value;
                    }
                    i = end + 1;
                    continue;
                }
            } else {
                // $VAR format
                while (end < str.length() && 
                       (std::isalnum(str[end]) || str[end] == '_')) {
                    end++;
                }
                if (end > start) {
                    std::string varName = str.substr(start, end - start);
                    const char* value = getenv(varName.c_str());
                    if (value) {
                        result += value;
                    }
                    i = end;
                    continue;
                }
            }
        }
        
        result += str[i];
        i++;
    }
    
    return Path(result);
}

std::string SanitizeFilename(const std::string& name) {
    std::string result;
    result.reserve(name.length());
    
    for (char c : name) {
        // Replace invalid characters with underscore
        if (c == '/' || c == '\\' || c == ':' || c == '*' || 
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|' ||
            c < 32) {
            result += '_';
        } else {
            result += c;
        }
    }
    
    // Trim leading/trailing spaces and dots
    while (!result.empty() && (result.front() == ' ' || result.front() == '.')) {
        result.erase(0, 1);
    }
    while (!result.empty() && (result.back() == ' ' || result.back() == '.')) {
        result.pop_back();
    }
    
    return result.empty() ? "_" : result;
}

Path UniqueFilename(const Path& path) {
    if (!Exists(path)) return path;
    
    Path parent = path.Parent();
    std::string stem = path.Stem();
    std::string ext = path.Extension();
    
    for (int i = 1; i < 10000; ++i) {
        std::ostringstream oss;
        oss << stem << "_" << i << ext;
        Path candidate = parent / oss.str();
        if (!Exists(candidate)) {
            return candidate;
        }
    }
    
    return Path();
}

std::string FileChecksum(const Path& path) {
    // Placeholder - would need SHA256 implementation
    // Return empty string for now
    (void)path;
    return "";
}

} // namespace fs
} // namespace util
} // namespace shurium
