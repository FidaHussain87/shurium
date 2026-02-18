// SHURIUM - Thread Pool
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Provides a flexible thread pool for async task execution:
// - Work-stealing queue
// - Futures for result retrieval
// - Task priorities
// - Graceful shutdown

#ifndef SHURIUM_UTIL_THREADPOOL_H
#define SHURIUM_UTIL_THREADPOOL_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace shurium {
namespace util {

// ============================================================================
// Task Priority
// ============================================================================

/// Task priority levels
enum class TaskPriority {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

// ============================================================================
// Thread Pool
// ============================================================================

/**
 * A thread pool for executing tasks asynchronously.
 * 
 * Features:
 * - Configurable number of worker threads
 * - Task priorities
 * - Future-based result retrieval
 * - Graceful shutdown with task completion
 */
class ThreadPool {
public:
    /// Configuration
    struct Config {
        size_t numThreads{0};       // 0 = hardware concurrency
        size_t maxQueueSize{10000}; // Maximum pending tasks
        std::string name{"pool"};   // Pool name for logging
        bool startImmediately{true};// Start workers on construction
    };
    
    /// Create with default configuration
    ThreadPool();
    
    /// Create with specified number of threads
    explicit ThreadPool(size_t numThreads);
    
    /// Create with configuration
    explicit ThreadPool(const Config& config);
    
    /// Destructor (waits for pending tasks)
    ~ThreadPool();
    
    // Non-copyable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    /// Start worker threads
    void Start();
    
    /// Stop accepting new tasks
    void Stop();
    
    /// Wait for all pending tasks to complete
    void Wait();
    
    /// Shutdown immediately (cancel pending tasks)
    void Shutdown();
    
    /// Check if pool is running
    bool IsRunning() const { return running_.load(); }
    
    /// Get number of worker threads
    size_t ThreadCount() const { return workers_.size(); }
    
    /// Get number of pending tasks
    size_t PendingTasks() const;
    
    /// Get number of active (executing) tasks
    size_t ActiveTasks() const { return activeTasks_.load(); }
    
    // ========================================================================
    // Task Submission
    // ========================================================================
    
    /**
     * Submit a task for execution.
     * 
     * @tparam F Callable type
     * @tparam Args Argument types
     * @param f Callable to execute
     * @param args Arguments to pass to callable
     * @return Future for the result
     */
    template<typename F, typename... Args>
    auto Submit(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        return SubmitWithPriority(TaskPriority::Normal,
                                   std::forward<F>(f),
                                   std::forward<Args>(args)...);
    }
    
    /**
     * Submit a task with specific priority.
     */
    template<typename F, typename... Args>
    auto SubmitWithPriority(TaskPriority priority, F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        
        using ReturnType = typename std::invoke_result<F, Args...>::type;
        
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<ReturnType> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            
            if (!running_.load()) {
                throw std::runtime_error("ThreadPool not running");
            }
            
            if (tasks_.size() >= config_.maxQueueSize) {
                throw std::runtime_error("ThreadPool queue full");
            }
            
            tasks_.emplace(priority, [task]() { (*task)(); });
        }
        
        condition_.notify_one();
        return result;
    }
    
    /**
     * Submit a task without caring about the result.
     */
    template<typename F, typename... Args>
    void Execute(F&& f, Args&&... args) {
        ExecuteWithPriority(TaskPriority::Normal,
                            std::forward<F>(f),
                            std::forward<Args>(args)...);
    }
    
    /**
     * Submit a task with priority without caring about result.
     */
    template<typename F, typename... Args>
    void ExecuteWithPriority(TaskPriority priority, F&& f, Args&&... args) {
        auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            
            if (!running_.load()) {
                throw std::runtime_error("ThreadPool not running");
            }
            
            if (tasks_.size() >= config_.maxQueueSize) {
                throw std::runtime_error("ThreadPool queue full");
            }
            
            tasks_.emplace(priority, std::move(func));
        }
        
        condition_.notify_one();
    }
    
    /**
     * Try to submit a task (returns false if queue full).
     */
    template<typename F, typename... Args>
    bool TrySubmit(F&& f, Args&&... args) {
        return TrySubmitWithPriority(TaskPriority::Normal,
                                      std::forward<F>(f),
                                      std::forward<Args>(args)...);
    }
    
    /**
     * Try to submit with priority.
     */
    template<typename F, typename... Args>
    bool TrySubmitWithPriority(TaskPriority priority, F&& f, Args&&... args) {
        auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            
            if (!running_.load() || tasks_.size() >= config_.maxQueueSize) {
                return false;
            }
            
            tasks_.emplace(priority, std::move(func));
        }
        
        condition_.notify_one();
        return true;
    }

private:
    /// Task with priority
    struct PrioritizedTask {
        TaskPriority priority;
        std::function<void()> task;
        
        PrioritizedTask(TaskPriority p, std::function<void()> t)
            : priority(p), task(std::move(t)) {}
        
        /// Comparison for priority queue (higher priority first)
        bool operator<(const PrioritizedTask& other) const {
            return priority < other.priority;
        }
    };
    
    Config config_;
    std::vector<std::thread> workers_;
    std::priority_queue<PrioritizedTask> tasks_;
    
    mutable std::mutex queueMutex_;
    std::condition_variable condition_;
    std::condition_variable waitCondition_;
    
    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};
    std::atomic<size_t> activeTasks_{0};
    
    /// Worker thread function
    void WorkerLoop();
};

// ============================================================================
// Global Thread Pool
// ============================================================================

/// Get the global thread pool instance
ThreadPool& GetGlobalThreadPool();

/// Initialize global thread pool with configuration
void InitGlobalThreadPool(const ThreadPool::Config& config);

/// Shutdown global thread pool
void ShutdownGlobalThreadPool();

// ============================================================================
// Parallel Algorithms
// ============================================================================

/**
 * Execute a function in parallel over a range.
 * 
 * @tparam Iterator Iterator type
 * @tparam Func Function type
 * @param begin Start iterator
 * @param end End iterator
 * @param func Function to apply to each element
 * @param pool Thread pool to use (default: global)
 */
template<typename Iterator, typename Func>
void ParallelFor(Iterator begin, Iterator end, Func func,
                  ThreadPool& pool = GetGlobalThreadPool()) {
    std::vector<std::future<void>> futures;
    
    for (auto it = begin; it != end; ++it) {
        futures.push_back(pool.Submit([&func, it]() { func(*it); }));
    }
    
    for (auto& f : futures) {
        f.get();
    }
}

/**
 * Execute a function in parallel over an index range.
 */
template<typename Func>
void ParallelForIndex(size_t begin, size_t end, Func func,
                       ThreadPool& pool = GetGlobalThreadPool()) {
    std::vector<std::future<void>> futures;
    
    for (size_t i = begin; i < end; ++i) {
        futures.push_back(pool.Submit([&func, i]() { func(i); }));
    }
    
    for (auto& f : futures) {
        f.get();
    }
}

/**
 * Map function over elements in parallel.
 */
template<typename InputIterator, typename OutputIterator, typename Func>
void ParallelMap(InputIterator inBegin, InputIterator inEnd,
                  OutputIterator outBegin, Func func,
                  ThreadPool& pool = GetGlobalThreadPool()) {
    std::vector<std::future<void>> futures;
    
    auto out = outBegin;
    for (auto in = inBegin; in != inEnd; ++in, ++out) {
        futures.push_back(pool.Submit([&func, in, out]() { *out = func(*in); }));
    }
    
    for (auto& f : futures) {
        f.get();
    }
}

// ============================================================================
// Task Group
// ============================================================================

/**
 * Group multiple tasks and wait for all to complete.
 */
class TaskGroup {
public:
    explicit TaskGroup(ThreadPool& pool = GetGlobalThreadPool());
    ~TaskGroup();
    
    // Non-copyable
    TaskGroup(const TaskGroup&) = delete;
    TaskGroup& operator=(const TaskGroup&) = delete;
    
    /**
     * Add a task to the group.
     */
    template<typename F, typename... Args>
    void Add(F&& f, Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++pendingCount_;
        
        pool_.Execute([this, func = std::bind(std::forward<F>(f),
                                               std::forward<Args>(args)...)]() {
            try {
                func();
            } catch (...) {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!hasException_) {
                    hasException_ = true;
                    exception_ = std::current_exception();
                }
            }
            
            {
                std::lock_guard<std::mutex> lock(mutex_);
                --pendingCount_;
            }
            condition_.notify_all();
        });
    }
    
    /// Wait for all tasks to complete
    void Wait();
    
    /// Wait with timeout (returns false on timeout)
    bool WaitFor(std::chrono::milliseconds timeout);
    
    /// Get number of pending tasks
    size_t PendingCount() const;
    
    /// Check if any task threw an exception
    bool HasException() const;
    
    /// Rethrow exception if any
    void RethrowException();

private:
    ThreadPool& pool_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    size_t pendingCount_{0};
    bool hasException_{false};
    std::exception_ptr exception_;
};

// ============================================================================
// Async/Await Helpers
// ============================================================================

/**
 * Run a function asynchronously.
 */
template<typename F, typename... Args>
auto Async(F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type> {
    return GetGlobalThreadPool().Submit(std::forward<F>(f),
                                         std::forward<Args>(args)...);
}

/**
 * Run a function asynchronously on specific pool.
 */
template<typename F, typename... Args>
auto AsyncOn(ThreadPool& pool, F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type> {
    return pool.Submit(std::forward<F>(f), std::forward<Args>(args)...);
}

/**
 * Wait for multiple futures.
 */
template<typename T>
std::vector<T> WaitAll(std::vector<std::future<T>>& futures) {
    std::vector<T> results;
    results.reserve(futures.size());
    
    for (auto& f : futures) {
        results.push_back(f.get());
    }
    
    return results;
}

/**
 * Wait for multiple void futures.
 */
inline void WaitAll(std::vector<std::future<void>>& futures) {
    for (auto& f : futures) {
        f.get();
    }
}

/**
 * Wait for first future to complete.
 * Returns index of completed future.
 */
template<typename T>
size_t WaitAny(std::vector<std::future<T>>& futures) {
    while (true) {
        for (size_t i = 0; i < futures.size(); ++i) {
            if (futures[i].wait_for(std::chrono::milliseconds(0)) 
                == std::future_status::ready) {
                return i;
            }
        }
        std::this_thread::yield();
    }
}

// ============================================================================
// Scheduled Tasks
// ============================================================================

/**
 * Scheduler for delayed and periodic tasks.
 */
class Scheduler {
public:
    Scheduler();
    explicit Scheduler(ThreadPool& pool);
    ~Scheduler();
    
    // Non-copyable
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    
    /// Start scheduler
    void Start();
    
    /// Stop scheduler
    void Stop();
    
    /// Check if running
    bool IsRunning() const { return running_.load(); }
    
    /**
     * Schedule a task to run after delay.
     * @return Task ID for cancellation
     */
    template<typename F, typename... Args>
    uint64_t ScheduleAfter(std::chrono::milliseconds delay, F&& f, Args&&... args) {
        auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        return ScheduleTask(std::chrono::steady_clock::now() + delay,
                            std::chrono::milliseconds(0),
                            std::move(func));
    }
    
    /**
     * Schedule a task to run at specific time.
     */
    template<typename F, typename... Args>
    uint64_t ScheduleAt(std::chrono::steady_clock::time_point time,
                         F&& f, Args&&... args) {
        auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        return ScheduleTask(time, std::chrono::milliseconds(0), std::move(func));
    }
    
    /**
     * Schedule a periodic task.
     * @param initialDelay Delay before first execution
     * @param period Time between executions
     */
    template<typename F, typename... Args>
    uint64_t SchedulePeriodic(std::chrono::milliseconds initialDelay,
                               std::chrono::milliseconds period,
                               F&& f, Args&&... args) {
        auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        return ScheduleTask(std::chrono::steady_clock::now() + initialDelay,
                            period, std::move(func));
    }
    
    /// Cancel a scheduled task
    bool Cancel(uint64_t taskId);
    
    /// Cancel all scheduled tasks
    void CancelAll();
    
    /// Get number of scheduled tasks
    size_t TaskCount() const;

private:
    struct ScheduledTask {
        uint64_t id;
        std::chrono::steady_clock::time_point nextRun;
        std::chrono::milliseconds period;
        std::function<void()> task;
        bool cancelled{false};
        
        bool operator>(const ScheduledTask& other) const {
            return nextRun > other.nextRun;
        }
    };
    
    ThreadPool* pool_;
    bool ownPool_{false};
    std::thread schedulerThread_;
    
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::priority_queue<ScheduledTask, std::vector<ScheduledTask>,
                        std::greater<ScheduledTask>> tasks_;
    
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> nextId_{1};
    
    /// Add task to queue
    uint64_t ScheduleTask(std::chrono::steady_clock::time_point time,
                           std::chrono::milliseconds period,
                           std::function<void()> func);
    
    /// Scheduler loop
    void SchedulerLoop();
};

} // namespace util
} // namespace shurium

#endif // SHURIUM_UTIL_THREADPOOL_H
