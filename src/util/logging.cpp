// SHURIUM - Logging Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/util/logging.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace shurium {
namespace util {

// ============================================================================
// Log Level Functions
// ============================================================================

const char* LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        case LogLevel::Off:   return "OFF";
        default:              return "UNKNOWN";
    }
}

LogLevel LogLevelFromString(const std::string& str) {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    
    if (upper == "TRACE") return LogLevel::Trace;
    if (upper == "DEBUG") return LogLevel::Debug;
    if (upper == "INFO")  return LogLevel::Info;
    if (upper == "WARN" || upper == "WARNING") return LogLevel::Warn;
    if (upper == "ERROR") return LogLevel::Error;
    if (upper == "FATAL") return LogLevel::Fatal;
    if (upper == "OFF")   return LogLevel::Off;
    
    return LogLevel::Info; // Default
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string FormatLogTimestamp(std::chrono::system_clock::time_point tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string GetThreadIdString() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

std::string FixedWidth(const std::string& str, size_t width, char pad) {
    if (str.length() >= width) {
        return str.substr(0, width);
    }
    return str + std::string(width - str.length(), pad);
}

std::string GetBasename(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

// ============================================================================
// ConsoleSink Implementation
// ============================================================================

ConsoleSink::ConsoleSink() = default;

ConsoleSink::ConsoleSink(const Config& config) : config_(config) {}

void ConsoleSink::Write(const LogEntry& entry) {
    if (entry.level < config_.level) {
        return;
    }
    
    std::string formatted = Format(entry);
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Determine output stream
    FILE* stream = stdout;
    if (config_.useStderr && entry.level >= LogLevel::Error) {
        stream = stderr;
    }
    
    // Write with optional color
    if (config_.useColors && isatty(fileno(stream))) {
        const char* colorCode = GetColorCode(entry.level);
        const char* resetCode = "\033[0m";
        fprintf(stream, "%s%s%s\n", colorCode, formatted.c_str(), resetCode);
    } else {
        fprintf(stream, "%s\n", formatted.c_str());
    }
}

void ConsoleSink::Flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    fflush(stdout);
    fflush(stderr);
}

std::string ConsoleSink::Format(const LogEntry& entry) const {
    std::ostringstream oss;
    
    if (config_.showTimestamp) {
        oss << FormatLogTimestamp(entry.timestamp) << " ";
    }
    
    if (config_.showLevel) {
        oss << "[" << FixedWidth(LogLevelToString(entry.level), 5) << "] ";
    }
    
    if (config_.showCategory && !entry.category.empty() && 
        entry.category != LogCategory::DEFAULT) {
        oss << "[" << entry.category << "] ";
    }
    
    if (config_.showThread) {
        std::ostringstream tid;
        tid << entry.threadId;
        oss << "[" << tid.str() << "] ";
    }
    
    if (config_.showLocation && !entry.file.empty()) {
        oss << GetBasename(entry.file) << ":" << entry.line << " ";
    }
    
    oss << entry.message;
    
    return oss.str();
}

const char* ConsoleSink::GetColorCode(LogLevel level) const {
    switch (level) {
        case LogLevel::Trace: return "\033[90m";      // Dark gray
        case LogLevel::Debug: return "\033[36m";      // Cyan
        case LogLevel::Info:  return "\033[32m";      // Green
        case LogLevel::Warn:  return "\033[33m";      // Yellow
        case LogLevel::Error: return "\033[31m";      // Red
        case LogLevel::Fatal: return "\033[35;1m";    // Bold magenta
        default:              return "\033[0m";       // Reset
    }
}

// ============================================================================
// FileSink Implementation
// ============================================================================

FileSink::FileSink() = default;

FileSink::FileSink(const std::string& path) {
    config_.path = path;
    Open(path);
}

FileSink::FileSink(const Config& config) : config_(config) {
    if (!config_.path.empty()) {
        Open(config_.path);
    }
}

FileSink::~FileSink() {
    Close();
}

bool FileSink::Open(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (file_.is_open()) {
        file_.close();
    }
    
    config_.path = path;
    
    auto mode = config_.append ? (std::ios::out | std::ios::app) : std::ios::out;
    file_.open(path, mode);
    
    if (file_.is_open()) {
        // Get current file size
        file_.seekp(0, std::ios::end);
        currentSize_ = static_cast<size_t>(file_.tellp());
        return true;
    }
    
    return false;
}

void FileSink::Close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.close();
    }
}

bool FileSink::IsOpen() const {
    return file_.is_open();
}

void FileSink::Write(const LogEntry& entry) {
    if (entry.level < config_.level) {
        return;
    }
    
    std::string formatted = Format(entry);
    formatted += "\n";
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!file_.is_open()) {
        return;
    }
    
    // Check if rotation is needed
    if (config_.rotate) {
        RotateIfNeeded();
    }
    
    file_ << formatted;
    currentSize_ += formatted.length();
    
    if (config_.autoFlush) {
        file_.flush();
    }
}

void FileSink::Flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.flush();
    }
}

void FileSink::SetConfig(const Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool pathChanged = config_.path != config.path;
    config_ = config;
    
    if (pathChanged && !config.path.empty()) {
        if (file_.is_open()) {
            file_.close();
        }
        auto mode = config_.append ? (std::ios::out | std::ios::app) : std::ios::out;
        file_.open(config_.path, mode);
        if (file_.is_open()) {
            file_.seekp(0, std::ios::end);
            currentSize_ = static_cast<size_t>(file_.tellp());
        }
    }
}

std::string FileSink::Format(const LogEntry& entry) const {
    std::ostringstream oss;
    
    if (config_.showTimestamp) {
        oss << FormatLogTimestamp(entry.timestamp) << " ";
    }
    
    if (config_.showLevel) {
        oss << "[" << FixedWidth(LogLevelToString(entry.level), 5) << "] ";
    }
    
    if (config_.showCategory && !entry.category.empty()) {
        oss << "[" << entry.category << "] ";
    }
    
    if (config_.showThread) {
        std::ostringstream tid;
        tid << entry.threadId;
        oss << "[" << tid.str() << "] ";
    }
    
    if (config_.showLocation && !entry.file.empty()) {
        oss << GetBasename(entry.file) << ":" << entry.line;
        if (!entry.function.empty()) {
            oss << " " << entry.function << "()";
        }
        oss << " ";
    }
    
    oss << entry.message;
    
    return oss.str();
}

void FileSink::RotateIfNeeded() {
    if (currentSize_ >= config_.maxSize) {
        Rotate();
    }
}

void FileSink::Rotate() {
    if (!file_.is_open()) {
        return;
    }
    
    file_.close();
    
    // Rotate existing files
    for (size_t i = config_.maxFiles - 1; i > 0; --i) {
        std::string oldName = config_.path + "." + std::to_string(i);
        std::string newName = config_.path + "." + std::to_string(i + 1);
        std::rename(oldName.c_str(), newName.c_str());
    }
    
    // Rename current file to .1
    std::string rotatedName = config_.path + ".1";
    std::rename(config_.path.c_str(), rotatedName.c_str());
    
    // Remove oldest file if necessary
    std::string oldestFile = config_.path + "." + std::to_string(config_.maxFiles + 1);
    std::remove(oldestFile.c_str());
    
    // Open new file
    file_.open(config_.path, std::ios::out);
    currentSize_ = 0;
}

// ============================================================================
// CallbackSink Implementation
// ============================================================================

CallbackSink::CallbackSink(Callback callback, LogLevel level)
    : callback_(std::move(callback)), level_(level) {}

void CallbackSink::Write(const LogEntry& entry) {
    if (entry.level < level_ || !callback_) {
        return;
    }
    callback_(entry);
}

// ============================================================================
// Logger Implementation
// ============================================================================

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() = default;

Logger::~Logger() {
    Shutdown();
}

void Logger::Initialize() {
    if (initialized_.exchange(true)) {
        return; // Already initialized
    }
    
    // Add default console sink
    auto consoleSink = std::make_shared<ConsoleSink>();
    AddSink(consoleSink);
    
    // Enable all categories by default
    allCategoriesEnabled_ = true;
}

void Logger::Shutdown() {
    if (!initialized_.exchange(false)) {
        return; // Not initialized
    }
    
    Flush();
    ClearSinks();
}

void Logger::AddSink(std::shared_ptr<ILogSink> sink) {
    std::lock_guard<std::mutex> lock(sinksMutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::RemoveSink(const std::shared_ptr<ILogSink>& sink) {
    std::lock_guard<std::mutex> lock(sinksMutex_);
    sinks_.erase(std::remove(sinks_.begin(), sinks_.end(), sink), sinks_.end());
}

void Logger::ClearSinks() {
    std::lock_guard<std::mutex> lock(sinksMutex_);
    sinks_.clear();
}

size_t Logger::SinkCount() const {
    std::lock_guard<std::mutex> lock(sinksMutex_);
    return sinks_.size();
}

void Logger::SetLevel(LogLevel level) {
    level_.store(level);
}

void Logger::EnableCategory(const std::string& category) {
    std::lock_guard<std::mutex> lock(categoriesMutex_);
    enabledCategories_.insert(category);
    allCategoriesEnabled_ = false;
}

void Logger::DisableCategory(const std::string& category) {
    std::lock_guard<std::mutex> lock(categoriesMutex_);
    enabledCategories_.erase(category);
}

bool Logger::IsCategoryEnabled(const std::string& category) const {
    if (allCategoriesEnabled_) {
        return true;
    }
    std::lock_guard<std::mutex> lock(categoriesMutex_);
    return enabledCategories_.find(category) != enabledCategories_.end();
}

void Logger::EnableAllCategories() {
    std::lock_guard<std::mutex> lock(categoriesMutex_);
    allCategoriesEnabled_ = true;
}

void Logger::DisableAllCategories() {
    std::lock_guard<std::mutex> lock(categoriesMutex_);
    allCategoriesEnabled_ = false;
    enabledCategories_.clear();
}

void Logger::Log(LogLevel level, const std::string& category,
                 const std::string& message,
                 const char* file, int line, const char* function) {
    if (!WillLog(level, category)) {
        return;
    }
    
    LogEntry entry;
    entry.level = level;
    entry.category = category;
    entry.message = message;
    entry.file = file ? file : "";
    entry.line = line;
    entry.function = function ? function : "";
    entry.timestamp = std::chrono::system_clock::now();
    entry.threadId = std::this_thread::get_id();
    
    // Send to all sinks
    std::lock_guard<std::mutex> lock(sinksMutex_);
    for (const auto& sink : sinks_) {
        sink->Write(entry);
    }
}

void Logger::LogF(LogLevel level, const std::string& category,
                  const char* file, int line, const char* function,
                  const char* format, ...) {
    if (!WillLog(level, category)) {
        return;
    }
    
    // Format message
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    Log(level, category, buffer, file, line, function);
}

bool Logger::WillLog(LogLevel level, const std::string& category) const {
    if (level < level_.load()) {
        return false;
    }
    return IsCategoryEnabled(category);
}

void Logger::Flush() {
    std::lock_guard<std::mutex> lock(sinksMutex_);
    for (const auto& sink : sinks_) {
        sink->Flush();
    }
}

// ============================================================================
// LogStream Implementation
// ============================================================================

LogStream::LogStream(LogLevel level, const std::string& category,
                     const char* file, int line, const char* function)
    : level_(level)
    , category_(category)
    , file_(file)
    , line_(line)
    , function_(function)
    , active_(Logger::Instance().WillLog(level, category)) {}

LogStream::~LogStream() {
    if (active_) {
        Logger::Instance().Log(level_, category_, stream_.str(),
                               file_, line_, function_);
    }
}

LogStream::LogStream(LogStream&& other) noexcept
    : stream_(std::move(other.stream_))
    , level_(other.level_)
    , category_(std::move(other.category_))
    , file_(other.file_)
    , line_(other.line_)
    , function_(other.function_)
    , active_(other.active_) {
    other.active_ = false;
}

// ============================================================================
// ScopedLogTimer Implementation
// ============================================================================

ScopedLogTimer::ScopedLogTimer(const std::string& category, 
                               const std::string& operation)
    : category_(category)
    , operation_(operation)
    , start_(std::chrono::steady_clock::now())
    , lastCheckpoint_(start_) {
    
    if (Logger::Instance().WillLog(LogLevel::Debug, category)) {
        Logger::Instance().Log(LogLevel::Debug, category,
                               "Starting: " + operation);
    }
}

ScopedLogTimer::~ScopedLogTimer() {
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start_);
    
    if (Logger::Instance().WillLog(LogLevel::Debug, category_)) {
        std::ostringstream oss;
        oss << "Completed: " << operation_ << " in " << duration.count() << "ms";
        Logger::Instance().Log(LogLevel::Debug, category_, oss.str());
    }
}

void ScopedLogTimer::Checkpoint(const std::string& name) {
    auto now = std::chrono::steady_clock::now();
    auto sinceLast = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastCheckpoint_);
    auto sinceStart = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - start_);
    lastCheckpoint_ = now;
    
    if (Logger::Instance().WillLog(LogLevel::Debug, category_)) {
        std::ostringstream oss;
        oss << "Checkpoint [" << name << "]: " 
            << sinceLast.count() << "ms (total: " << sinceStart.count() << "ms)";
        Logger::Instance().Log(LogLevel::Debug, category_, oss.str());
    }
}

} // namespace util
} // namespace shurium
