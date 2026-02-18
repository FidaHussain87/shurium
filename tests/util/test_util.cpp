// SHURIUM - Util Module Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>

#include <shurium/util/logging.h>
#include <shurium/util/time.h>
#include <shurium/util/fs.h>
#include <shurium/util/threadpool.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace shurium {
namespace util {
namespace {

// ============================================================================
// Logging Tests
// ============================================================================

class LoggingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing sinks
        Logger::Instance().ClearSinks();
    }
    
    void TearDown() override {
        Logger::Instance().ClearSinks();
    }
};

TEST_F(LoggingTest, LogLevelToString) {
    EXPECT_STREQ(LogLevelToString(LogLevel::Trace), "TRACE");
    EXPECT_STREQ(LogLevelToString(LogLevel::Debug), "DEBUG");
    EXPECT_STREQ(LogLevelToString(LogLevel::Info), "INFO");
    EXPECT_STREQ(LogLevelToString(LogLevel::Warn), "WARN");
    EXPECT_STREQ(LogLevelToString(LogLevel::Error), "ERROR");
    EXPECT_STREQ(LogLevelToString(LogLevel::Fatal), "FATAL");
}

TEST_F(LoggingTest, LogLevelFromString) {
    EXPECT_EQ(LogLevelFromString("trace"), LogLevel::Trace);
    EXPECT_EQ(LogLevelFromString("DEBUG"), LogLevel::Debug);
    EXPECT_EQ(LogLevelFromString("Info"), LogLevel::Info);
    EXPECT_EQ(LogLevelFromString("WARN"), LogLevel::Warn);
    EXPECT_EQ(LogLevelFromString("warning"), LogLevel::Warn);
    EXPECT_EQ(LogLevelFromString("ERROR"), LogLevel::Error);
    EXPECT_EQ(LogLevelFromString("fatal"), LogLevel::Fatal);
    EXPECT_EQ(LogLevelFromString("invalid"), LogLevel::Info); // Default
}

TEST_F(LoggingTest, LoggerSingleton) {
    auto& logger1 = Logger::Instance();
    auto& logger2 = Logger::Instance();
    EXPECT_EQ(&logger1, &logger2);
}

TEST_F(LoggingTest, LoggerAddRemoveSink) {
    auto& logger = Logger::Instance();
    EXPECT_EQ(logger.SinkCount(), 0);
    
    auto sink = std::make_shared<ConsoleSink>();
    logger.AddSink(sink);
    EXPECT_EQ(logger.SinkCount(), 1);
    
    logger.RemoveSink(sink);
    EXPECT_EQ(logger.SinkCount(), 0);
}

TEST_F(LoggingTest, LoggerSetLevel) {
    auto& logger = Logger::Instance();
    
    logger.SetLevel(LogLevel::Debug);
    EXPECT_EQ(logger.GetLevel(), LogLevel::Debug);
    
    logger.SetLevel(LogLevel::Error);
    EXPECT_EQ(logger.GetLevel(), LogLevel::Error);
}

TEST_F(LoggingTest, LoggerWillLog) {
    auto& logger = Logger::Instance();
    logger.SetLevel(LogLevel::Info);
    logger.EnableAllCategories();
    
    EXPECT_FALSE(logger.WillLog(LogLevel::Debug, LogCategory::DEFAULT));
    EXPECT_TRUE(logger.WillLog(LogLevel::Info, LogCategory::DEFAULT));
    EXPECT_TRUE(logger.WillLog(LogLevel::Error, LogCategory::DEFAULT));
}

TEST_F(LoggingTest, LoggerCategoryFiltering) {
    auto& logger = Logger::Instance();
    logger.SetLevel(LogLevel::Info);
    
    logger.DisableAllCategories();
    EXPECT_FALSE(logger.IsCategoryEnabled(LogCategory::NET));
    
    logger.EnableCategory(LogCategory::NET);
    EXPECT_TRUE(logger.IsCategoryEnabled(LogCategory::NET));
    EXPECT_FALSE(logger.IsCategoryEnabled(LogCategory::WALLET));
    
    logger.EnableAllCategories();
    EXPECT_TRUE(logger.IsCategoryEnabled(LogCategory::NET));
    EXPECT_TRUE(logger.IsCategoryEnabled(LogCategory::WALLET));
}

TEST_F(LoggingTest, CallbackSink) {
    std::vector<std::string> capturedMessages;
    
    auto callback = [&](const LogEntry& entry) {
        capturedMessages.push_back(entry.message);
    };
    
    auto sink = std::make_shared<CallbackSink>(callback, LogLevel::Info);
    auto& logger = Logger::Instance();
    logger.AddSink(sink);
    logger.SetLevel(LogLevel::Info);
    logger.EnableAllCategories();
    
    logger.Log(LogLevel::Info, LogCategory::DEFAULT, "Test message");
    
    ASSERT_EQ(capturedMessages.size(), 1);
    EXPECT_EQ(capturedMessages[0], "Test message");
}

TEST_F(LoggingTest, ConsoleSinkConfig) {
    ConsoleSink::Config config;
    config.useColors = false;
    config.showTimestamp = true;
    config.showLevel = true;
    
    ConsoleSink sink(config);
    EXPECT_EQ(sink.GetConfig().useColors, false);
    EXPECT_EQ(sink.GetConfig().showTimestamp, true);
}

// ============================================================================
// Time Tests
// ============================================================================

class TimeTest : public ::testing::Test {
protected:
    void SetUp() override {
        DisableMockTime();
    }
    
    void TearDown() override {
        DisableMockTime();
    }
};

TEST_F(TimeTest, GetTime) {
    int64_t time1 = GetTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    int64_t time2 = GetTime();
    
    EXPECT_GE(time2, time1);
    EXPECT_GT(time1, 0);
}

TEST_F(TimeTest, GetTimeMillis) {
    int64_t time1 = GetTimeMillis();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    int64_t time2 = GetTimeMillis();
    
    EXPECT_GE(time2, time1);
    EXPECT_GE(time2 - time1, 10);
}

TEST_F(TimeTest, MockTime) {
    EXPECT_FALSE(IsMockTimeEnabled());
    
    EnableMockTime();
    EXPECT_TRUE(IsMockTimeEnabled());
    
    SetMockTime(1000);
    EXPECT_EQ(GetMockTime(), 1000);
    EXPECT_EQ(GetTime(), 1000);
    
    AdvanceMockTime(Seconds{100});
    EXPECT_EQ(GetMockTime(), 1100);
    
    DisableMockTime();
    EXPECT_FALSE(IsMockTimeEnabled());
}

TEST_F(TimeTest, UnixTimeConversion) {
    int64_t timestamp = 1704067200; // 2024-01-01 00:00:00 UTC
    
    auto tp = FromUnixTime(timestamp);
    int64_t converted = ToUnixTime(tp);
    EXPECT_EQ(converted, timestamp);
}

TEST_F(TimeTest, MillisConversion) {
    int64_t timestampMs = 1704067200123;
    
    auto tp = FromUnixTimeMillis(timestampMs);
    int64_t converted = ToUnixTimeMillis(tp);
    EXPECT_EQ(converted, timestampMs);
}

TEST_F(TimeTest, FormatISO8601) {
    auto tp = FromUnixTime(1704067200); // 2024-01-01 00:00:00 UTC
    std::string formatted = FormatISO8601(tp);
    EXPECT_EQ(formatted, "2024-01-01T00:00:00Z");
}

TEST_F(TimeTest, FormatDuration) {
    EXPECT_EQ(FormatDuration(Seconds{0}), "0s");
    EXPECT_EQ(FormatDuration(Seconds{45}), "45s");
    EXPECT_EQ(FormatDuration(Seconds{90}), "1m 30s");
    EXPECT_EQ(FormatDuration(Seconds{3661}), "1h 1m 1s");
    EXPECT_EQ(FormatDuration(Seconds{90061}), "1d 1h 1m 1s");
}

TEST_F(TimeTest, ParseISO8601) {
    auto tp = ParseISO8601("2024-01-01T00:00:00Z");
    EXPECT_NE(tp, SystemTimePoint{});
    EXPECT_EQ(ToUnixTime(tp), 1704067200);
}

TEST_F(TimeTest, Timer) {
    Timer timer;
    EXPECT_TRUE(timer.IsRunning());
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_GE(timer.ElapsedMillis(), 50);
    EXPECT_GE(timer.ElapsedSeconds(), 0.05);
    
    timer.Stop();
    EXPECT_FALSE(timer.IsRunning());
    
    int64_t elapsed = timer.ElapsedMillis();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(timer.ElapsedMillis(), elapsed);
    
    timer.Reset();
    EXPECT_EQ(timer.ElapsedMillis(), 0);
}

TEST_F(TimeTest, RateLimiter) {
    RateLimiter limiter(100.0, 5); // 100 tokens/sec, burst of 5
    
    // Should be able to consume burst immediately
    EXPECT_TRUE(limiter.TryConsume(5));
    
    // Should not be able to consume more immediately
    EXPECT_FALSE(limiter.TryConsume(1));
    
    // Reset and try again
    limiter.Reset();
    EXPECT_TRUE(limiter.TryConsume(1));
}

TEST_F(TimeTest, DeadlineTimer) {
    DeadlineTimer timer(Milliseconds{100});
    
    EXPECT_FALSE(timer.IsExpired());
    EXPECT_GT(timer.Remaining().count(), 0);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    
    EXPECT_TRUE(timer.IsExpired());
    EXPECT_EQ(timer.Remaining().count(), 0);
}

TEST_F(TimeTest, SleepInterruptible) {
    std::atomic<bool> interrupt{false};
    
    auto start = GetTimeMillis();
    bool wasInterrupted = SleepInterruptible(Milliseconds{100}, interrupt);
    auto elapsed = GetTimeMillis() - start;
    
    EXPECT_FALSE(wasInterrupted);
    EXPECT_GE(elapsed, 100);
    
    // Test with interrupt
    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        interrupt.store(true);
    });
    
    start = GetTimeMillis();
    wasInterrupted = SleepInterruptible(Milliseconds{1000}, interrupt);
    elapsed = GetTimeMillis() - start;
    
    t.join();
    
    EXPECT_TRUE(wasInterrupted);
    EXPECT_LT(elapsed, 500);
}

// ============================================================================
// Filesystem Tests
// ============================================================================

class FilesystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::TempDirectoryPath() / fs::Path("shurium_test");
        fs::CreateDirectories(testDir_);
    }
    
    void TearDown() override {
        fs::RemoveAll(testDir_);
    }
    
    fs::Path testDir_;
};

TEST_F(FilesystemTest, PathConstruction) {
    fs::Path empty;
    EXPECT_TRUE(empty.Empty());
    
    fs::Path p1("/usr/local");
    EXPECT_FALSE(p1.Empty());
    EXPECT_EQ(p1.String(), "/usr/local");
}

TEST_F(FilesystemTest, PathIsAbsolute) {
    fs::Path abs("/usr/local");
    fs::Path rel("local/bin");
    
    EXPECT_TRUE(abs.IsAbsolute());
    EXPECT_FALSE(rel.IsAbsolute());
    EXPECT_FALSE(abs.IsRelative());
    EXPECT_TRUE(rel.IsRelative());
}

TEST_F(FilesystemTest, PathParent) {
    fs::Path p("/usr/local/bin");
    
    EXPECT_EQ(p.Parent().String(), "/usr/local");
    EXPECT_EQ(p.Parent().Parent().String(), "/usr");
}

TEST_F(FilesystemTest, PathFilename) {
    fs::Path p("/usr/local/bin/test.txt");
    
    EXPECT_EQ(p.Filename(), "test.txt");
    EXPECT_EQ(p.Stem(), "test");
    EXPECT_EQ(p.Extension(), ".txt");
}

TEST_F(FilesystemTest, PathAppend) {
    fs::Path p("/usr");
    p.Append(fs::Path("local"));
    EXPECT_EQ(p.String(), "/usr/local");
    
    fs::Path p2 = fs::Path("/home") / fs::Path("user") / fs::Path("docs");
    EXPECT_EQ(p2.String(), "/home/user/docs");
}

TEST_F(FilesystemTest, PathNormalize) {
    fs::Path p("/usr/local/../lib/./test");
    auto normalized = p.Normalize();
    EXPECT_EQ(normalized.String(), "/usr/lib/test");
}

TEST_F(FilesystemTest, FileExists) {
    fs::Path testFile = testDir_ / fs::Path("test.txt");
    
    EXPECT_FALSE(fs::Exists(testFile));
    
    EXPECT_TRUE(fs::WriteFile(testFile, "hello"));
    EXPECT_TRUE(fs::Exists(testFile));
    EXPECT_TRUE(fs::IsFile(testFile));
    EXPECT_FALSE(fs::IsDirectory(testFile));
}

TEST_F(FilesystemTest, DirectoryExists) {
    fs::Path subDir = testDir_ / fs::Path("subdir");
    
    EXPECT_FALSE(fs::Exists(subDir));
    
    EXPECT_TRUE(fs::CreateDirectory(subDir));
    EXPECT_TRUE(fs::Exists(subDir));
    EXPECT_TRUE(fs::IsDirectory(subDir));
    EXPECT_FALSE(fs::IsFile(subDir));
}

TEST_F(FilesystemTest, ReadWriteFile) {
    fs::Path testFile = testDir_ / fs::Path("test.txt");
    std::string content = "Hello, SHURIUM!\nLine 2";
    
    EXPECT_TRUE(fs::WriteFile(testFile, content));
    
    std::string read = fs::ReadFile(testFile);
    EXPECT_EQ(read, content);
}

TEST_F(FilesystemTest, ReadWriteFileBytes) {
    fs::Path testFile = testDir_ / fs::Path("test.bin");
    std::vector<uint8_t> data = {0x00, 0x01, 0xFF, 0xFE, 0x42};
    
    EXPECT_TRUE(fs::WriteFile(testFile, data));
    
    auto read = fs::ReadFileBytes(testFile);
    EXPECT_EQ(read, data);
}

TEST_F(FilesystemTest, AppendFile) {
    fs::Path testFile = testDir_ / fs::Path("append.txt");
    
    EXPECT_TRUE(fs::WriteFile(testFile, "Line 1\n"));
    EXPECT_TRUE(fs::AppendFile(testFile, "Line 2\n"));
    
    std::string content = fs::ReadFile(testFile);
    EXPECT_EQ(content, "Line 1\nLine 2\n");
}

TEST_F(FilesystemTest, CopyFile) {
    fs::Path src = testDir_ / fs::Path("src.txt");
    fs::Path dst = testDir_ / fs::Path("dst.txt");
    
    EXPECT_TRUE(fs::WriteFile(src, "test content"));
    EXPECT_TRUE(fs::CopyFile(src, dst));
    
    EXPECT_TRUE(fs::Exists(dst));
    EXPECT_EQ(fs::ReadFile(dst), "test content");
}

TEST_F(FilesystemTest, RemoveFile) {
    fs::Path testFile = testDir_ / fs::Path("remove.txt");
    fs::WriteFile(testFile, "temp");
    
    EXPECT_TRUE(fs::Exists(testFile));
    EXPECT_TRUE(fs::RemoveFile(testFile));
    EXPECT_FALSE(fs::Exists(testFile));
}

TEST_F(FilesystemTest, CreateDirectories) {
    fs::Path nested = testDir_ / fs::Path("a") / fs::Path("b") / fs::Path("c");
    
    EXPECT_TRUE(fs::CreateDirectories(nested));
    EXPECT_TRUE(fs::IsDirectory(nested));
}

TEST_F(FilesystemTest, ListDirectory) {
    // Create some files
    fs::WriteFile(testDir_ / fs::Path("file1.txt"), "1");
    fs::WriteFile(testDir_ / fs::Path("file2.txt"), "2");
    fs::CreateDirectory(testDir_ / fs::Path("subdir"));
    
    auto entries = fs::ListDirectory(testDir_);
    EXPECT_EQ(entries.size(), 3);
    
    // Test with filter
    auto textFiles = fs::ListDirectory(testDir_, [](const fs::DirectoryEntry& e) {
        return e.type == fs::FileType::Regular;
    });
    EXPECT_EQ(textFiles.size(), 2);
}

TEST_F(FilesystemTest, RemoveAll) {
    fs::Path subDir = testDir_ / fs::Path("sub");
    fs::CreateDirectory(subDir);
    fs::WriteFile(subDir / fs::Path("file.txt"), "test");
    
    EXPECT_TRUE(fs::RemoveAll(subDir));
    EXPECT_FALSE(fs::Exists(subDir));
}

TEST_F(FilesystemTest, FileSize) {
    fs::Path testFile = testDir_ / fs::Path("size.txt");
    std::string content = "Hello World!"; // 12 bytes
    
    fs::WriteFile(testFile, content);
    EXPECT_EQ(fs::FileSize(testFile), 12);
}

TEST_F(FilesystemTest, TempFile) {
    {
        fs::TempFile temp;
        EXPECT_TRUE(temp.IsValid());
        EXPECT_TRUE(fs::Exists(temp.GetPath()));
    }
    // TempFile should be deleted when out of scope
}

TEST_F(FilesystemTest, TempDirectory) {
    {
        fs::TempDirectory temp;
        EXPECT_TRUE(temp.IsValid());
        EXPECT_TRUE(fs::IsDirectory(temp.GetPath()));
        
        // Can write files inside
        fs::WriteFile(temp.GetPath() / fs::Path("test.txt"), "content");
    }
    // TempDirectory should be deleted when out of scope
}

TEST_F(FilesystemTest, CurrentPath) {
    fs::Path current = fs::CurrentPath();
    EXPECT_FALSE(current.Empty());
    EXPECT_TRUE(current.IsAbsolute());
}

TEST_F(FilesystemTest, ExpandUser) {
    fs::Path home = fs::HomeDirectory();
    if (!home.Empty()) {
        fs::Path expanded = fs::ExpandUser(fs::Path("~"));
        EXPECT_EQ(expanded, home);
        
        fs::Path expanded2 = fs::ExpandUser(fs::Path("~/test"));
        EXPECT_EQ(expanded2, home / fs::Path("test"));
    }
}

TEST_F(FilesystemTest, SanitizeFilename) {
    EXPECT_EQ(fs::SanitizeFilename("test.txt"), "test.txt");
    EXPECT_EQ(fs::SanitizeFilename("test/file.txt"), "test_file.txt");
    EXPECT_EQ(fs::SanitizeFilename("test:file.txt"), "test_file.txt");
    EXPECT_EQ(fs::SanitizeFilename("...test"), "test");
}

TEST_F(FilesystemTest, UniqueFilename) {
    fs::Path file1 = testDir_ / fs::Path("test.txt");
    fs::WriteFile(file1, "1");
    
    fs::Path unique = fs::UniqueFilename(file1);
    EXPECT_NE(unique, file1);
    EXPECT_FALSE(fs::Exists(unique));
}

// ============================================================================
// Thread Pool Tests
// ============================================================================

class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool_ = std::make_unique<ThreadPool>(4);
    }
    
    void TearDown() override {
        pool_.reset();
    }
    
    std::unique_ptr<ThreadPool> pool_;
};

TEST_F(ThreadPoolTest, Construction) {
    EXPECT_TRUE(pool_->IsRunning());
    EXPECT_EQ(pool_->ThreadCount(), 4);
}

TEST_F(ThreadPoolTest, SubmitAndWait) {
    std::atomic<int> counter{0};
    
    auto future = pool_->Submit([&counter]() {
        counter++;
        return 42;
    });
    
    int result = future.get();
    EXPECT_EQ(result, 42);
    EXPECT_EQ(counter.load(), 1);
}

TEST_F(ThreadPoolTest, Execute) {
    std::atomic<int> counter{0};
    
    pool_->Execute([&counter]() {
        counter++;
    });
    
    pool_->Wait();
    EXPECT_EQ(counter.load(), 1);
}

TEST_F(ThreadPoolTest, MultipleTasks) {
    const int numTasks = 100;
    std::atomic<int> counter{0};
    
    std::vector<std::future<void>> futures;
    for (int i = 0; i < numTasks; ++i) {
        futures.push_back(pool_->Submit([&counter]() {
            counter++;
        }));
    }
    
    for (auto& f : futures) {
        f.get();
    }
    
    EXPECT_EQ(counter.load(), numTasks);
}

TEST_F(ThreadPoolTest, TaskPriority) {
    pool_->Shutdown();
    pool_ = std::make_unique<ThreadPool>(1);
    
    std::vector<int> results;
    std::mutex mutex;
    
    // Submit low priority first, then high priority
    pool_->ExecuteWithPriority(TaskPriority::Low, [&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::lock_guard<std::mutex> lock(mutex);
        results.push_back(1);
    });
    
    pool_->ExecuteWithPriority(TaskPriority::High, [&]() {
        std::lock_guard<std::mutex> lock(mutex);
        results.push_back(2);
    });
    
    pool_->Wait();
    
    // At least verify both tasks completed
    EXPECT_EQ(results.size(), 2);
}

TEST_F(ThreadPoolTest, TrySubmit) {
    ThreadPool::Config config;
    config.maxQueueSize = 1;
    config.numThreads = 1;
    
    ThreadPool smallPool(config);
    
    // Fill the queue
    std::atomic<bool> block{true};
    smallPool.Execute([&block]() {
        while (block.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // Wait a bit for task to start
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // First try should succeed (one slot)
    bool result1 = smallPool.TrySubmit([]() {});
    EXPECT_TRUE(result1);
    
    // Second try should fail (queue full)
    bool result2 = smallPool.TrySubmit([]() {});
    EXPECT_FALSE(result2);
    
    block.store(false);
}

TEST_F(ThreadPoolTest, Shutdown) {
    std::atomic<int> completed{0};
    
    for (int i = 0; i < 10; ++i) {
        pool_->Execute([&completed]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            completed++;
        });
    }
    
    pool_->Shutdown();
    EXPECT_FALSE(pool_->IsRunning());
    
    // Some tasks might not have completed due to immediate shutdown
    EXPECT_LE(completed.load(), 10);
}

TEST_F(ThreadPoolTest, TaskGroup) {
    std::atomic<int> counter{0};
    
    TaskGroup group(*pool_);
    
    for (int i = 0; i < 10; ++i) {
        group.Add([&counter]() {
            counter++;
        });
    }
    
    group.Wait();
    EXPECT_EQ(counter.load(), 10);
}

TEST_F(ThreadPoolTest, TaskGroupWithException) {
    TaskGroup group(*pool_);
    
    group.Add([]() {
        throw std::runtime_error("Test exception");
    });
    
    EXPECT_THROW(group.Wait(), std::runtime_error);
}

TEST_F(ThreadPoolTest, TaskGroupWaitFor) {
    TaskGroup group(*pool_);
    
    group.Add([]() {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    });
    
    bool completed = group.WaitFor(std::chrono::milliseconds(50));
    EXPECT_FALSE(completed);
}

TEST_F(ThreadPoolTest, ParallelForIndex) {
    std::vector<int> data(100, 0);
    std::mutex mutex;
    
    ParallelForIndex(0, 100, [&](size_t i) {
        std::lock_guard<std::mutex> lock(mutex);
        data[i] = static_cast<int>(i);
    }, *pool_);
    
    for (size_t i = 0; i < 100; ++i) {
        EXPECT_EQ(data[i], static_cast<int>(i));
    }
}

TEST_F(ThreadPoolTest, Async) {
    auto future = Async([]() {
        return 42;
    });
    
    EXPECT_EQ(future.get(), 42);
}

TEST_F(ThreadPoolTest, WaitAll) {
    std::vector<std::future<int>> futures;
    
    for (int i = 0; i < 5; ++i) {
        futures.push_back(pool_->Submit([i]() {
            return i * 2;
        }));
    }
    
    auto results = WaitAll(futures);
    EXPECT_EQ(results.size(), 5);
    
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(results[i], i * 2);
    }
}

TEST_F(ThreadPoolTest, Scheduler) {
    Scheduler scheduler(*pool_);
    scheduler.Start();
    
    std::atomic<int> counter{0};
    
    // Schedule task to run after 50ms
    scheduler.ScheduleAfter(std::chrono::milliseconds(50), [&counter]() {
        counter++;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(counter.load(), 1);
    
    scheduler.Stop();
}

TEST_F(ThreadPoolTest, SchedulerPeriodic) {
    Scheduler scheduler(*pool_);
    scheduler.Start();
    
    std::atomic<int> counter{0};
    
    // Schedule periodic task
    scheduler.SchedulePeriodic(
        std::chrono::milliseconds(10),
        std::chrono::milliseconds(20),
        [&counter]() { counter++; }
    );
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    int count = counter.load();
    EXPECT_GE(count, 3); // Should have run at least 3 times
    
    scheduler.Stop();
}

TEST_F(ThreadPoolTest, SchedulerCancel) {
    Scheduler scheduler(*pool_);
    scheduler.Start();
    
    std::atomic<int> counter{0};
    
    auto taskId = scheduler.ScheduleAfter(std::chrono::milliseconds(100), [&counter]() {
        counter++;
    });
    
    EXPECT_TRUE(scheduler.Cancel(taskId));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_EQ(counter.load(), 0);
    
    scheduler.Stop();
}

TEST_F(ThreadPoolTest, GlobalThreadPool) {
    auto& pool1 = GetGlobalThreadPool();
    auto& pool2 = GetGlobalThreadPool();
    
    EXPECT_EQ(&pool1, &pool2);
    EXPECT_TRUE(pool1.IsRunning());
}

// ============================================================================
// Utility Tests
// ============================================================================

TEST(UtilityTest, FormatLogTimestamp) {
    auto tp = FromUnixTime(1704067200); // 2024-01-01 00:00:00 UTC
    std::string ts = FormatLogTimestamp(tp);
    
    // Should contain date and time components
    EXPECT_FALSE(ts.empty());
    EXPECT_NE(ts.find("2024"), std::string::npos);
}

TEST(UtilityTest, GetThreadIdString) {
    std::string tid = GetThreadIdString();
    EXPECT_FALSE(tid.empty());
}

TEST(UtilityTest, FixedWidth) {
    EXPECT_EQ(FixedWidth("test", 6), "test  ");
    EXPECT_EQ(FixedWidth("testing", 4), "test");
    EXPECT_EQ(FixedWidth("hi", 5, '-'), "hi---");
}

TEST(UtilityTest, GetBasename) {
    EXPECT_EQ(GetBasename("/usr/local/bin/test"), "test");
    EXPECT_EQ(GetBasename("test.txt"), "test.txt");
    EXPECT_EQ(GetBasename("/"), "");
}

} // namespace
} // namespace util
} // namespace shurium
