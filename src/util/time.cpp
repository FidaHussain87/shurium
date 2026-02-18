// SHURIUM - Time Utilities Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/util/time.h"

#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>

namespace shurium {
namespace util {

// ============================================================================
// Mock Time State
// ============================================================================

namespace {
    std::atomic<bool> g_mockTimeEnabled{false};
    std::atomic<int64_t> g_mockTime{0};
    std::mutex g_mockTimeMutex;
}

// ============================================================================
// Unix Timestamps
// ============================================================================

int64_t GetTime() {
    if (g_mockTimeEnabled.load()) {
        return g_mockTime.load();
    }
    return std::chrono::duration_cast<Seconds>(
        SystemClock::now().time_since_epoch()).count();
}

int64_t GetTimeMillis() {
    if (g_mockTimeEnabled.load()) {
        return g_mockTime.load() * MILLIS_PER_SECOND;
    }
    return std::chrono::duration_cast<Milliseconds>(
        SystemClock::now().time_since_epoch()).count();
}

int64_t GetTimeMicros() {
    if (g_mockTimeEnabled.load()) {
        return g_mockTime.load() * MICROS_PER_SECOND;
    }
    return std::chrono::duration_cast<Microseconds>(
        SystemClock::now().time_since_epoch()).count();
}

SystemTimePoint GetSystemTime() {
    if (g_mockTimeEnabled.load()) {
        return FromUnixTime(g_mockTime.load());
    }
    return SystemClock::now();
}

SteadyTimePoint GetSteadyTime() {
    return SteadyClock::now();
}

SystemTimePoint FromUnixTime(int64_t timestamp) {
    return SystemTimePoint{Seconds{timestamp}};
}

int64_t ToUnixTime(SystemTimePoint tp) {
    return std::chrono::duration_cast<Seconds>(tp.time_since_epoch()).count();
}

SystemTimePoint FromUnixTimeMillis(int64_t timestampMs) {
    return SystemTimePoint{Milliseconds{timestampMs}};
}

int64_t ToUnixTimeMillis(SystemTimePoint tp) {
    return std::chrono::duration_cast<Milliseconds>(tp.time_since_epoch()).count();
}

// ============================================================================
// Time Formatting
// ============================================================================

std::string FormatISO8601(SystemTimePoint tp) {
    auto time = SystemClock::to_time_t(tp);
    std::tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &time);
#else
    gmtime_r(&time, &tm_buf);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string FormatISO8601Millis(SystemTimePoint tp) {
    auto time = SystemClock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<Milliseconds>(
        tp.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &time);
#else
    gmtime_r(&time, &tm_buf);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

std::string FormatHTTPDate(SystemTimePoint tp) {
    auto time = SystemClock::to_time_t(tp);
    std::tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &time);
#else
    gmtime_r(&time, &tm_buf);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%a, %d %b %Y %H:%M:%S GMT");
    return oss.str();
}

std::string FormatLog(SystemTimePoint tp) {
    auto time = SystemClock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<Milliseconds>(
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

std::string FormatTime(SystemTimePoint tp, const char* format) {
    auto time = SystemClock::to_time_t(tp);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, format);
    return oss.str();
}

std::string FormatDuration(Seconds duration) {
    int64_t total = duration.count();
    
    if (total < 0) {
        return "-" + FormatDuration(Seconds{-total});
    }
    
    if (total == 0) {
        return "0s";
    }
    
    int64_t days = total / SECONDS_PER_DAY;
    total %= SECONDS_PER_DAY;
    int64_t hours = total / SECONDS_PER_HOUR;
    total %= SECONDS_PER_HOUR;
    int64_t minutes = total / SECONDS_PER_MINUTE;
    int64_t seconds = total % SECONDS_PER_MINUTE;
    
    std::ostringstream oss;
    if (days > 0) oss << days << "d ";
    if (hours > 0) oss << hours << "h ";
    if (minutes > 0) oss << minutes << "m ";
    if (seconds > 0) oss << seconds << "s";
    
    std::string result = oss.str();
    // Trim trailing space
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    return result;
}

std::string FormatDurationMillis(Milliseconds duration) {
    int64_t total = duration.count();
    
    if (total < 0) {
        return "-" + FormatDurationMillis(Milliseconds{-total});
    }
    
    if (total == 0) {
        return "0s";
    }
    
    int64_t totalSeconds = total / MILLIS_PER_SECOND;
    int64_t millis = total % MILLIS_PER_SECOND;
    
    int64_t days = totalSeconds / SECONDS_PER_DAY;
    totalSeconds %= SECONDS_PER_DAY;
    int64_t hours = totalSeconds / SECONDS_PER_HOUR;
    totalSeconds %= SECONDS_PER_HOUR;
    int64_t minutes = totalSeconds / SECONDS_PER_MINUTE;
    int64_t seconds = totalSeconds % SECONDS_PER_MINUTE;
    
    std::ostringstream oss;
    if (days > 0) oss << days << "d ";
    if (hours > 0) oss << hours << "h ";
    if (minutes > 0) oss << minutes << "m ";
    
    if (seconds > 0 || millis > 0) {
        oss << seconds;
        if (millis > 0) {
            oss << '.' << std::setfill('0') << std::setw(3) << millis;
        }
        oss << "s";
    }
    
    std::string result = oss.str();
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    return result;
}

std::string FormatRelativeTime(SystemTimePoint tp) {
    auto now = GetSystemTime();
    auto diff = std::chrono::duration_cast<Seconds>(now - tp).count();
    
    bool future = diff < 0;
    if (future) diff = -diff;
    
    std::string unit;
    int64_t value;
    
    if (diff < 60) {
        value = diff;
        unit = "second";
    } else if (diff < 3600) {
        value = diff / 60;
        unit = "minute";
    } else if (diff < 86400) {
        value = diff / 3600;
        unit = "hour";
    } else if (diff < 2592000) { // ~30 days
        value = diff / 86400;
        unit = "day";
    } else if (diff < 31536000) { // ~365 days
        value = diff / 2592000;
        unit = "month";
    } else {
        value = diff / 31536000;
        unit = "year";
    }
    
    std::ostringstream oss;
    if (future) {
        oss << "in " << value << " " << unit;
    } else {
        oss << value << " " << unit;
    }
    if (value != 1) oss << "s";
    if (!future) oss << " ago";
    
    return oss.str();
}

// ============================================================================
// Time Parsing
// ============================================================================

SystemTimePoint ParseISO8601(const std::string& str) {
    std::tm tm_buf = {};
    
    // Try parsing with milliseconds first
    std::istringstream iss(str);
    iss >> std::get_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    
    if (iss.fail()) {
        // Try without T separator
        iss.clear();
        iss.str(str);
        iss >> std::get_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    }
    
    if (iss.fail()) {
        return SystemTimePoint{};
    }
    
    // Handle milliseconds if present
    int millis = 0;
    char c;
    if (iss >> c && c == '.') {
        iss >> millis;
    }
    
#ifdef _WIN32
    auto time = _mkgmtime(&tm_buf);
#else
    auto time = timegm(&tm_buf);
#endif
    
    if (time == -1) {
        return SystemTimePoint{};
    }
    
    return SystemTimePoint{Seconds{time}} + Milliseconds{millis};
}

SystemTimePoint ParseHTTPDate(const std::string& str) {
    std::tm tm_buf = {};
    std::istringstream iss(str);
    
    // Try RFC 1123 format: "Mon, 15 Jan 2024 10:30:00 GMT"
    iss >> std::get_time(&tm_buf, "%a, %d %b %Y %H:%M:%S");
    
    if (iss.fail()) {
        // Try RFC 850 format: "Monday, 15-Jan-24 10:30:00 GMT"
        iss.clear();
        iss.str(str);
        iss >> std::get_time(&tm_buf, "%A, %d-%b-%y %H:%M:%S");
    }
    
    if (iss.fail()) {
        // Try asctime format: "Mon Jan 15 10:30:00 2024"
        iss.clear();
        iss.str(str);
        iss >> std::get_time(&tm_buf, "%c");
    }
    
    if (iss.fail()) {
        return SystemTimePoint{};
    }
    
#ifdef _WIN32
    auto time = _mkgmtime(&tm_buf);
#else
    auto time = timegm(&tm_buf);
#endif
    
    if (time == -1) {
        return SystemTimePoint{};
    }
    
    return SystemTimePoint{Seconds{time}};
}

SystemTimePoint ParseTime(const std::string& str, const char* format) {
    std::tm tm_buf = {};
    std::istringstream iss(str);
    iss >> std::get_time(&tm_buf, format);
    
    if (iss.fail()) {
        return SystemTimePoint{};
    }
    
    auto time = std::mktime(&tm_buf);
    if (time == -1) {
        return SystemTimePoint{};
    }
    
    return SystemTimePoint{Seconds{time}};
}

SystemTimePoint ParseAuto(const std::string& str) {
    // Try various formats
    auto result = ParseISO8601(str);
    if (result != SystemTimePoint{}) return result;
    
    result = ParseHTTPDate(str);
    if (result != SystemTimePoint{}) return result;
    
    // Try common formats
    const char* formats[] = {
        "%Y-%m-%d",
        "%Y/%m/%d",
        "%d/%m/%Y",
        "%m/%d/%Y",
        "%Y-%m-%d %H:%M",
        "%Y-%m-%d %H:%M:%S",
    };
    
    for (const auto* fmt : formats) {
        result = ParseTime(str, fmt);
        if (result != SystemTimePoint{}) return result;
    }
    
    return SystemTimePoint{};
}

// ============================================================================
// Mock Time
// ============================================================================

void EnableMockTime() {
    std::lock_guard<std::mutex> lock(g_mockTimeMutex);
    g_mockTimeEnabled.store(true);
    if (g_mockTime.load() == 0) {
        g_mockTime.store(GetTime());
    }
}

void DisableMockTime() {
    std::lock_guard<std::mutex> lock(g_mockTimeMutex);
    g_mockTimeEnabled.store(false);
}

bool IsMockTimeEnabled() {
    return g_mockTimeEnabled.load();
}

void SetMockTime(int64_t timestamp) {
    g_mockTime.store(timestamp);
}

void SetMockTime(SystemTimePoint tp) {
    g_mockTime.store(ToUnixTime(tp));
}

void AdvanceMockTime(Seconds duration) {
    g_mockTime.fetch_add(duration.count());
}

int64_t GetMockTime() {
    return g_mockTime.load();
}

// ============================================================================
// Timer Implementation
// ============================================================================

Timer::Timer() : running_(false) {
    Start();
}

void Timer::Start() {
    if (!running_) {
        start_ = SteadyClock::now();
        running_ = true;
    }
}

void Timer::Stop() {
    if (running_) {
        auto now = SteadyClock::now();
        accumulated_ += std::chrono::duration_cast<Nanoseconds>(now - start_);
        running_ = false;
    }
}

void Timer::Reset() {
    accumulated_ = Nanoseconds{0};
    running_ = false;
}

double Timer::ElapsedSeconds() const {
    auto ns = Elapsed();
    return static_cast<double>(ns.count()) / NANOS_PER_SECOND;
}

int64_t Timer::ElapsedMillis() const {
    return std::chrono::duration_cast<Milliseconds>(Elapsed()).count();
}

int64_t Timer::ElapsedMicros() const {
    return std::chrono::duration_cast<Microseconds>(Elapsed()).count();
}

Nanoseconds Timer::Elapsed() const {
    auto total = accumulated_;
    if (running_) {
        total += std::chrono::duration_cast<Nanoseconds>(
            SteadyClock::now() - start_);
    }
    return total;
}

// ============================================================================
// RateLimiter Implementation
// ============================================================================

RateLimiter::RateLimiter(double rate, size_t burst)
    : rate_(rate)
    , burst_(burst)
    , tokens_(static_cast<double>(burst))
    , lastUpdate_(SteadyClock::now()) {}

bool RateLimiter::TryConsume() {
    return TryConsume(1);
}

bool RateLimiter::TryConsume(size_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    Refill();
    
    if (tokens_ >= static_cast<double>(count)) {
        tokens_ -= static_cast<double>(count);
        return true;
    }
    return false;
}

void RateLimiter::Wait() {
    Wait(1);
}

void RateLimiter::Wait(size_t count) {
    while (!TryConsume(count)) {
        // Calculate wait time
        double needed = static_cast<double>(count);
        double waitSeconds = (needed - Available()) / rate_;
        if (waitSeconds > 0) {
            std::this_thread::sleep_for(
                std::chrono::microseconds(static_cast<int64_t>(waitSeconds * 1000000)));
        }
    }
}

double RateLimiter::Available() const {
    // Note: Not perfectly accurate without lock, but good enough for checking
    auto now = SteadyClock::now();
    auto elapsed = std::chrono::duration<double>(now - lastUpdate_).count();
    double available = tokens_ + elapsed * rate_;
    return std::min(available, static_cast<double>(burst_));
}

void RateLimiter::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    tokens_ = static_cast<double>(burst_);
    lastUpdate_ = SteadyClock::now();
}

void RateLimiter::SetRate(double rate) {
    std::lock_guard<std::mutex> lock(mutex_);
    Refill();
    rate_ = rate;
}

void RateLimiter::SetBurst(size_t burst) {
    std::lock_guard<std::mutex> lock(mutex_);
    burst_ = burst;
    if (tokens_ > static_cast<double>(burst)) {
        tokens_ = static_cast<double>(burst);
    }
}

void RateLimiter::Refill() {
    auto now = SteadyClock::now();
    auto elapsed = std::chrono::duration<double>(now - lastUpdate_).count();
    tokens_ = std::min(tokens_ + elapsed * rate_, static_cast<double>(burst_));
    lastUpdate_ = now;
}

// ============================================================================
// DeadlineTimer Implementation
// ============================================================================

DeadlineTimer::DeadlineTimer(Milliseconds timeout)
    : deadline_(SteadyClock::now() + timeout) {}

DeadlineTimer::DeadlineTimer(SteadyTimePoint deadline)
    : deadline_(deadline) {}

bool DeadlineTimer::IsExpired() const {
    return SteadyClock::now() >= deadline_;
}

Milliseconds DeadlineTimer::Remaining() const {
    auto now = SteadyClock::now();
    if (now >= deadline_) {
        return Milliseconds{0};
    }
    return std::chrono::duration_cast<Milliseconds>(deadline_ - now);
}

void DeadlineTimer::Extend(Milliseconds duration) {
    deadline_ += duration;
}

void DeadlineTimer::Reset(Milliseconds timeout) {
    deadline_ = SteadyClock::now() + timeout;
}

// ============================================================================
// Sleep Functions
// ============================================================================

void Sleep(Milliseconds duration) {
    std::this_thread::sleep_for(duration);
}

void SleepSeconds(int64_t seconds) {
    std::this_thread::sleep_for(Seconds{seconds});
}

void SleepMillis(int64_t milliseconds) {
    std::this_thread::sleep_for(Milliseconds{milliseconds});
}

void SleepUntil(SystemTimePoint tp) {
    auto now = SystemClock::now();
    if (tp > now) {
        std::this_thread::sleep_for(tp - now);
    }
}

void SleepUntil(SteadyTimePoint tp) {
    std::this_thread::sleep_until(tp);
}

bool SleepInterruptible(Milliseconds duration, const std::atomic<bool>& interrupt) {
    auto deadline = SteadyClock::now() + duration;
    
    while (SteadyClock::now() < deadline) {
        if (interrupt.load()) {
            return true;
        }
        // Sleep in small increments
        auto remaining = std::chrono::duration_cast<Milliseconds>(
            deadline - SteadyClock::now());
        auto sleepTime = std::min(remaining, Milliseconds{10});
        if (sleepTime > Milliseconds{0}) {
            std::this_thread::sleep_for(sleepTime);
        }
    }
    
    return interrupt.load();
}

} // namespace util
} // namespace shurium
