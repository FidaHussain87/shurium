// SHURIUM - Time Utilities
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Provides time-related utilities:
// - Unix timestamps
// - System clock and steady clock helpers
// - Time formatting and parsing
// - Mock time for testing

#ifndef SHURIUM_UTIL_TIME_H
#define SHURIUM_UTIL_TIME_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

namespace shurium {
namespace util {

// ============================================================================
// Type Aliases
// ============================================================================

/// Duration types
using Seconds = std::chrono::seconds;
using Milliseconds = std::chrono::milliseconds;
using Microseconds = std::chrono::microseconds;
using Nanoseconds = std::chrono::nanoseconds;
using Minutes = std::chrono::minutes;
using Hours = std::chrono::hours;

/// Time point types
using SystemClock = std::chrono::system_clock;
using SteadyClock = std::chrono::steady_clock;
using SystemTimePoint = std::chrono::system_clock::time_point;
using SteadyTimePoint = std::chrono::steady_clock::time_point;

// ============================================================================
// Unix Timestamps
// ============================================================================

/// Get current Unix timestamp in seconds
int64_t GetTime();

/// Get current Unix timestamp in milliseconds
int64_t GetTimeMillis();

/// Get current Unix timestamp in microseconds
int64_t GetTimeMicros();

/// Get system time point for current time
SystemTimePoint GetSystemTime();

/// Get steady time point for current time (monotonic)
SteadyTimePoint GetSteadyTime();

/// Convert Unix timestamp to system time point
SystemTimePoint FromUnixTime(int64_t timestamp);

/// Convert system time point to Unix timestamp
int64_t ToUnixTime(SystemTimePoint tp);

/// Convert millisecond timestamp to system time point
SystemTimePoint FromUnixTimeMillis(int64_t timestampMs);

/// Convert system time point to millisecond timestamp
int64_t ToUnixTimeMillis(SystemTimePoint tp);

// ============================================================================
// Duration Helpers
// ============================================================================

/// Count seconds between two time points
template<typename Clock>
int64_t SecondsBetween(std::chrono::time_point<Clock> start,
                        std::chrono::time_point<Clock> end) {
    return std::chrono::duration_cast<Seconds>(end - start).count();
}

/// Count milliseconds between two time points
template<typename Clock>
int64_t MillisBetween(std::chrono::time_point<Clock> start,
                       std::chrono::time_point<Clock> end) {
    return std::chrono::duration_cast<Milliseconds>(end - start).count();
}

/// Count microseconds between two time points
template<typename Clock>
int64_t MicrosBetween(std::chrono::time_point<Clock> start,
                       std::chrono::time_point<Clock> end) {
    return std::chrono::duration_cast<Microseconds>(end - start).count();
}

/// Check if duration has elapsed since start
template<typename Duration, typename Clock>
bool HasElapsed(std::chrono::time_point<Clock> start, Duration d) {
    return Clock::now() >= start + d;
}

// ============================================================================
// Time Formatting
// ============================================================================

/// Format time point as ISO 8601 string (e.g., "2024-01-15T10:30:00Z")
std::string FormatISO8601(SystemTimePoint tp);

/// Format time point as ISO 8601 with milliseconds
std::string FormatISO8601Millis(SystemTimePoint tp);

/// Format time point as HTTP date (e.g., "Mon, 15 Jan 2024 10:30:00 GMT")
std::string FormatHTTPDate(SystemTimePoint tp);

/// Format time point for logging (e.g., "2024-01-15 10:30:00.123")
std::string FormatLog(SystemTimePoint tp);

/// Format time point with custom strftime format
std::string FormatTime(SystemTimePoint tp, const char* format);

/// Format duration as human-readable string (e.g., "1h 23m 45s")
std::string FormatDuration(Seconds duration);

/// Format duration with milliseconds (e.g., "1h 23m 45.678s")
std::string FormatDurationMillis(Milliseconds duration);

/// Format relative time (e.g., "2 hours ago", "in 5 minutes")
std::string FormatRelativeTime(SystemTimePoint tp);

// ============================================================================
// Time Parsing
// ============================================================================

/// Parse ISO 8601 string to time point
/// Returns epoch on failure
SystemTimePoint ParseISO8601(const std::string& str);

/// Parse HTTP date to time point
SystemTimePoint ParseHTTPDate(const std::string& str);

/// Parse time string with custom format
SystemTimePoint ParseTime(const std::string& str, const char* format);

/// Try to parse various common date formats
SystemTimePoint ParseAuto(const std::string& str);

// ============================================================================
// Mock Time (for testing)
// ============================================================================

/// Enable mock time mode
void EnableMockTime();

/// Disable mock time mode
void DisableMockTime();

/// Check if mock time is enabled
bool IsMockTimeEnabled();

/// Set mock time (only works if mock time is enabled)
void SetMockTime(int64_t timestamp);

/// Set mock time from time point
void SetMockTime(SystemTimePoint tp);

/// Advance mock time by duration
void AdvanceMockTime(Seconds duration);

/// Get mock time (returns 0 if not enabled)
int64_t GetMockTime();

// ============================================================================
// Timer
// ============================================================================

/// Simple timer for measuring elapsed time
class Timer {
public:
    /// Create and start timer
    Timer();
    
    /// Start or restart timer
    void Start();
    
    /// Stop timer
    void Stop();
    
    /// Reset timer to zero
    void Reset();
    
    /// Check if timer is running
    bool IsRunning() const { return running_; }
    
    /// Get elapsed time in seconds
    double ElapsedSeconds() const;
    
    /// Get elapsed time in milliseconds
    int64_t ElapsedMillis() const;
    
    /// Get elapsed time in microseconds
    int64_t ElapsedMicros() const;
    
    /// Get elapsed duration
    Nanoseconds Elapsed() const;

private:
    SteadyTimePoint start_;
    Nanoseconds accumulated_{0};
    bool running_{false};
};

// ============================================================================
// Rate Limiter
// ============================================================================

/// Token bucket rate limiter
class RateLimiter {
public:
    /// Create rate limiter
    /// @param rate Tokens per second
    /// @param burst Maximum burst size
    RateLimiter(double rate, size_t burst);
    
    /// Try to consume a token
    /// @return true if token was consumed
    bool TryConsume();
    
    /// Try to consume multiple tokens
    bool TryConsume(size_t count);
    
    /// Wait until a token is available
    void Wait();
    
    /// Wait until multiple tokens are available
    void Wait(size_t count);
    
    /// Get current number of available tokens
    double Available() const;
    
    /// Reset rate limiter
    void Reset();
    
    /// Set rate
    void SetRate(double rate);
    
    /// Get rate
    double GetRate() const { return rate_; }
    
    /// Set burst size
    void SetBurst(size_t burst);
    
    /// Get burst size
    size_t GetBurst() const { return burst_; }

private:
    double rate_;           // Tokens per second
    size_t burst_;          // Maximum tokens
    double tokens_;         // Current tokens
    SteadyTimePoint lastUpdate_;
    mutable std::mutex mutex_;
    
    /// Refill tokens based on elapsed time
    void Refill();
};

// ============================================================================
// Deadline Timer
// ============================================================================

/// Timer that tracks time until a deadline
class DeadlineTimer {
public:
    /// Create timer with deadline
    explicit DeadlineTimer(Milliseconds timeout);
    
    /// Create timer with specific deadline time
    explicit DeadlineTimer(SteadyTimePoint deadline);
    
    /// Check if deadline has passed
    bool IsExpired() const;
    
    /// Get remaining time until deadline
    Milliseconds Remaining() const;
    
    /// Get deadline time point
    SteadyTimePoint GetDeadline() const { return deadline_; }
    
    /// Extend deadline
    void Extend(Milliseconds duration);
    
    /// Reset with new timeout
    void Reset(Milliseconds timeout);

private:
    SteadyTimePoint deadline_;
};

// ============================================================================
// Sleep Functions
// ============================================================================

/// Sleep for specified duration
void Sleep(Milliseconds duration);

/// Sleep for specified seconds
void SleepSeconds(int64_t seconds);

/// Sleep for specified milliseconds
void SleepMillis(int64_t milliseconds);

/// Sleep until specified time point
void SleepUntil(SystemTimePoint tp);

/// Sleep until specified steady time point
void SleepUntil(SteadyTimePoint tp);

/// Interruptible sleep (returns true if interrupted)
bool SleepInterruptible(Milliseconds duration, const std::atomic<bool>& interrupt);

// ============================================================================
// Constants
// ============================================================================

/// Seconds per minute
constexpr int64_t SECONDS_PER_MINUTE = 60;

/// Seconds per hour
constexpr int64_t SECONDS_PER_HOUR = 3600;

/// Seconds per day
constexpr int64_t SECONDS_PER_DAY = 86400;

/// Seconds per week
constexpr int64_t SECONDS_PER_WEEK = 604800;

/// Milliseconds per second
constexpr int64_t MILLIS_PER_SECOND = 1000;

/// Microseconds per second
constexpr int64_t MICROS_PER_SECOND = 1000000;

/// Nanoseconds per second
constexpr int64_t NANOS_PER_SECOND = 1000000000;

/// Unix epoch as system time point
inline SystemTimePoint UnixEpoch() {
    return SystemTimePoint{};
}

} // namespace util
} // namespace shurium

#endif // SHURIUM_UTIL_TIME_H
