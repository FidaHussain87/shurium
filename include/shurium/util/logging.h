// SHURIUM - Logging System
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Provides a flexible logging system with:
// - Multiple log levels (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)
// - Log categories for filtering
// - Console and file output
// - Thread-safe logging
// - Printf-style and stream-style interfaces

#ifndef SHURIUM_UTIL_LOGGING_H
#define SHURIUM_UTIL_LOGGING_H

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace shurium {
namespace util {

// ============================================================================
// Log Levels
// ============================================================================

enum class LogLevel {
    Trace = 0,   // Very detailed debugging
    Debug = 1,   // Debug information
    Info = 2,    // General information
    Warn = 3,    // Warnings
    Error = 4,   // Errors
    Fatal = 5,   // Fatal errors
    Off = 6      // Disable logging
};

/// Convert log level to string
const char* LogLevelToString(LogLevel level);

/// Parse log level from string
LogLevel LogLevelFromString(const std::string& str);

// ============================================================================
// Log Categories
// ============================================================================

/// Predefined log categories
namespace LogCategory {
    constexpr const char* DEFAULT = "default";
    constexpr const char* NET = "net";
    constexpr const char* MEMPOOL = "mempool";
    constexpr const char* VALIDATION = "validation";
    constexpr const char* WALLET = "wallet";
    constexpr const char* RPC = "rpc";
    constexpr const char* CONSENSUS = "consensus";
    constexpr const char* MINING = "mining";
    constexpr const char* IDENTITY = "identity";
    constexpr const char* UBI = "ubi";
    constexpr const char* DB = "db";
    constexpr const char* LOCK = "lock";
    constexpr const char* BENCH = "bench";
}

// ============================================================================
// Log Entry
// ============================================================================

/// A single log entry
struct LogEntry {
    LogLevel level;
    std::string category;
    std::string message;
    std::string file;
    int line;
    std::string function;
    std::chrono::system_clock::time_point timestamp;
    std::thread::id threadId;
    
    LogEntry() : level(LogLevel::Info), line(0) {}
};

// ============================================================================
// Log Sink Interface
// ============================================================================

/// Abstract base class for log output destinations
class ILogSink {
public:
    virtual ~ILogSink() = default;
    
    /// Write a log entry
    virtual void Write(const LogEntry& entry) = 0;
    
    /// Flush any buffered output
    virtual void Flush() = 0;
    
    /// Set minimum log level for this sink
    virtual void SetLevel(LogLevel level) = 0;
    
    /// Get minimum log level for this sink
    virtual LogLevel GetLevel() const = 0;
};

// ============================================================================
// Console Sink
// ============================================================================

/// Log sink that writes to console (stdout/stderr)
class ConsoleSink : public ILogSink {
public:
    /// Configuration
    struct Config {
        bool useColors{true};           // Use ANSI color codes
        bool useStderr{false};          // Write errors to stderr
        bool showTimestamp{true};       // Include timestamp
        bool showLevel{true};           // Include log level
        bool showCategory{true};        // Include category
        bool showThread{false};         // Include thread ID
        bool showLocation{false};       // Include file:line
        LogLevel level{LogLevel::Info}; // Minimum level
    };
    
    ConsoleSink();
    explicit ConsoleSink(const Config& config);
    
    void Write(const LogEntry& entry) override;
    void Flush() override;
    void SetLevel(LogLevel level) override { config_.level = level; }
    LogLevel GetLevel() const override { return config_.level; }
    
    /// Set configuration
    void SetConfig(const Config& config) { config_ = config; }
    const Config& GetConfig() const { return config_; }

private:
    Config config_;
    std::mutex mutex_;
    
    /// Format entry for output
    std::string Format(const LogEntry& entry) const;
    
    /// Get ANSI color code for level
    const char* GetColorCode(LogLevel level) const;
};

// ============================================================================
// File Sink
// ============================================================================

/// Log sink that writes to a file
class FileSink : public ILogSink {
public:
    /// Configuration
    struct Config {
        std::string path;               // Log file path
        bool append{true};              // Append to existing file
        bool autoFlush{false};          // Flush after each write
        size_t maxSize{10 * 1024 * 1024}; // Max file size (10 MB default)
        size_t maxFiles{5};             // Max rotated files to keep
        bool rotate{true};              // Enable log rotation
        bool showTimestamp{true};
        bool showLevel{true};
        bool showCategory{true};
        bool showThread{true};
        bool showLocation{true};
        LogLevel level{LogLevel::Debug};
    };
    
    FileSink();
    explicit FileSink(const std::string& path);
    explicit FileSink(const Config& config);
    ~FileSink() override;
    
    /// Open log file
    bool Open(const std::string& path);
    
    /// Close log file
    void Close();
    
    /// Check if file is open
    bool IsOpen() const;
    
    void Write(const LogEntry& entry) override;
    void Flush() override;
    void SetLevel(LogLevel level) override { config_.level = level; }
    LogLevel GetLevel() const override { return config_.level; }
    
    /// Set configuration
    void SetConfig(const Config& config);
    const Config& GetConfig() const { return config_; }
    
    /// Get current file size
    size_t GetCurrentSize() const { return currentSize_; }

private:
    Config config_;
    std::ofstream file_;
    std::mutex mutex_;
    size_t currentSize_{0};
    
    /// Format entry for output
    std::string Format(const LogEntry& entry) const;
    
    /// Rotate log files if needed
    void RotateIfNeeded();
    
    /// Perform log rotation
    void Rotate();
};

// ============================================================================
// Callback Sink
// ============================================================================

/// Log sink that calls a callback function
class CallbackSink : public ILogSink {
public:
    using Callback = std::function<void(const LogEntry&)>;
    
    CallbackSink() = default;
    explicit CallbackSink(Callback callback, LogLevel level = LogLevel::Info);
    
    void Write(const LogEntry& entry) override;
    void Flush() override {}
    void SetLevel(LogLevel level) override { level_ = level; }
    LogLevel GetLevel() const override { return level_; }
    
    void SetCallback(Callback callback) { callback_ = std::move(callback); }

private:
    Callback callback_;
    LogLevel level_{LogLevel::Info};
};

// ============================================================================
// Logger
// ============================================================================

/// Main logger class
class Logger {
public:
    /// Get the singleton instance
    static Logger& Instance();
    
    /// Initialize logger with default configuration
    void Initialize();
    
    /// Shutdown logger
    void Shutdown();
    
    /// Add a log sink
    void AddSink(std::shared_ptr<ILogSink> sink);
    
    /// Remove a log sink
    void RemoveSink(const std::shared_ptr<ILogSink>& sink);
    
    /// Clear all sinks
    void ClearSinks();
    
    /// Get number of sinks
    size_t SinkCount() const;
    
    /// Set global minimum log level
    void SetLevel(LogLevel level);
    
    /// Get global minimum log level
    LogLevel GetLevel() const { return level_.load(); }
    
    /// Enable a category
    void EnableCategory(const std::string& category);
    
    /// Disable a category
    void DisableCategory(const std::string& category);
    
    /// Check if category is enabled
    bool IsCategoryEnabled(const std::string& category) const;
    
    /// Enable all categories
    void EnableAllCategories();
    
    /// Disable all categories
    void DisableAllCategories();
    
    /// Log a message
    void Log(LogLevel level, const std::string& category,
             const std::string& message,
             const char* file = nullptr, int line = 0,
             const char* function = nullptr);
    
    /// Log with printf-style formatting
    void LogF(LogLevel level, const std::string& category,
              const char* file, int line, const char* function,
              const char* format, ...);
    
    /// Check if a message would be logged
    bool WillLog(LogLevel level, const std::string& category) const;
    
    /// Flush all sinks
    void Flush();

private:
    Logger();
    ~Logger();
    
    // Non-copyable
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::vector<std::shared_ptr<ILogSink>> sinks_;
    mutable std::mutex sinksMutex_;
    
    std::atomic<LogLevel> level_{LogLevel::Info};
    std::unordered_set<std::string> enabledCategories_;
    mutable std::mutex categoriesMutex_;
    bool allCategoriesEnabled_{true};
    
    std::atomic<bool> initialized_{false};
};

// ============================================================================
// Log Stream
// ============================================================================

/// Stream-style logging helper
class LogStream {
public:
    LogStream(LogLevel level, const std::string& category,
              const char* file, int line, const char* function);
    ~LogStream();
    
    // Non-copyable but movable
    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;
    LogStream(LogStream&& other) noexcept;
    LogStream& operator=(LogStream&& other) = delete;
    
    /// Stream insertion operators
    template<typename T>
    LogStream& operator<<(const T& value) {
        if (active_) {
            stream_ << value;
        }
        return *this;
    }
    
    /// Support for manipulators (endl, etc.)
    LogStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
        if (active_) {
            manip(stream_);
        }
        return *this;
    }

private:
    std::ostringstream stream_;
    LogLevel level_;
    std::string category_;
    const char* file_;
    int line_;
    const char* function_;
    bool active_;
};

// ============================================================================
// Logging Macros
// ============================================================================

/// Get logger instance
#define SHURIUM_LOGGER ::shurium::util::Logger::Instance()

/// Check if logging is enabled
#define SHURIUM_LOG_ENABLED(level, category) \
    SHURIUM_LOGGER.WillLog(::shurium::util::LogLevel::level, category)

/// Log with level and category
#define SHURIUM_LOG(level, category) \
    if (SHURIUM_LOG_ENABLED(level, category)) \
        ::shurium::util::LogStream(::shurium::util::LogLevel::level, category, \
                                  __FILE__, __LINE__, __func__)

/// Log to default category
#define SHURIUM_LOG_DEFAULT(level) \
    SHURIUM_LOG(level, ::shurium::util::LogCategory::DEFAULT)

/// Convenience macros for each level
#define LOG_TRACE(category)   SHURIUM_LOG(Trace, category)
#define LOG_DEBUG(category)   SHURIUM_LOG(Debug, category)
#define LOG_INFO(category)    SHURIUM_LOG(Info, category)
#define LOG_WARN(category)    SHURIUM_LOG(Warn, category)
#define LOG_ERROR(category)   SHURIUM_LOG(Error, category)
#define LOG_FATAL(category)   SHURIUM_LOG(Fatal, category)

/// Default category convenience macros
#define LogTrace()  LOG_TRACE(::shurium::util::LogCategory::DEFAULT)
#define LogDebug()  LOG_DEBUG(::shurium::util::LogCategory::DEFAULT)
#define LogInfo()   LOG_INFO(::shurium::util::LogCategory::DEFAULT)
#define LogWarn()   LOG_WARN(::shurium::util::LogCategory::DEFAULT)
#define LogError()  LOG_ERROR(::shurium::util::LogCategory::DEFAULT)
#define LogFatal()  LOG_FATAL(::shurium::util::LogCategory::DEFAULT)

/// Printf-style logging
#define SHURIUM_LOGF(level, category, ...) \
    do { \
        if (SHURIUM_LOG_ENABLED(level, category)) { \
            SHURIUM_LOGGER.LogF(::shurium::util::LogLevel::level, category, \
                              __FILE__, __LINE__, __func__, __VA_ARGS__); \
        } \
    } while(0)

#define LogTraceF(category, ...)  SHURIUM_LOGF(Trace, category, __VA_ARGS__)
#define LogDebugF(category, ...)  SHURIUM_LOGF(Debug, category, __VA_ARGS__)
#define LogInfoF(category, ...)   SHURIUM_LOGF(Info, category, __VA_ARGS__)
#define LogWarnF(category, ...)   SHURIUM_LOGF(Warn, category, __VA_ARGS__)
#define LogErrorF(category, ...)  SHURIUM_LOGF(Error, category, __VA_ARGS__)
#define LogFatalF(category, ...)  SHURIUM_LOGF(Fatal, category, __VA_ARGS__)

// ============================================================================
// Scoped Log Timer
// ============================================================================

/// RAII timer for logging execution time
class ScopedLogTimer {
public:
    ScopedLogTimer(const std::string& category, const std::string& operation);
    ~ScopedLogTimer();
    
    /// Mark a checkpoint
    void Checkpoint(const std::string& name);

private:
    std::string category_;
    std::string operation_;
    std::chrono::steady_clock::time_point start_;
    std::chrono::steady_clock::time_point lastCheckpoint_;
};

/// Macro for scoped timing
#define SHURIUM_LOG_TIMER(category, operation) \
    ::shurium::util::ScopedLogTimer _nexus_timer_##__LINE__(category, operation)

// ============================================================================
// Utility Functions
// ============================================================================

/// Format timestamp for logging
std::string FormatLogTimestamp(std::chrono::system_clock::time_point tp);

/// Get current thread ID as string
std::string GetThreadIdString();

/// Truncate or pad string to fixed width
std::string FixedWidth(const std::string& str, size_t width, char pad = ' ');

/// Get basename from file path
std::string GetBasename(const std::string& path);

} // namespace util
} // namespace shurium

#endif // SHURIUM_UTIL_LOGGING_H
