// SHURIUM - Thread Pool Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/util/threadpool.h"

#include <algorithm>

namespace shurium {
namespace util {

// ============================================================================
// ThreadPool Implementation
// ============================================================================

ThreadPool::ThreadPool() : ThreadPool(Config{}) {}

ThreadPool::ThreadPool(size_t numThreads) {
    config_.numThreads = numThreads;
    if (config_.startImmediately) {
        Start();
    }
}

ThreadPool::ThreadPool(const Config& config) : config_(config) {
    if (config_.startImmediately) {
        Start();
    }
}

ThreadPool::~ThreadPool() {
    Shutdown();
}

void ThreadPool::Start() {
    if (running_.exchange(true)) {
        return; // Already running
    }
    
    stopping_.store(false);
    
    size_t numThreads = config_.numThreads;
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) {
            numThreads = 2; // Fallback
        }
    }
    
    workers_.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back(&ThreadPool::WorkerLoop, this);
    }
}

void ThreadPool::Stop() {
    stopping_.store(true);
}

void ThreadPool::Wait() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    waitCondition_.wait(lock, [this] {
        return tasks_.empty() && activeTasks_.load() == 0;
    });
}

void ThreadPool::Shutdown() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        if (!running_.load()) {
            return;
        }
        running_.store(false);
        stopping_.store(true);
    }
    
    // Wake up all workers
    condition_.notify_all();
    
    // Join all threads
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
    
    // Clear remaining tasks
    std::priority_queue<PrioritizedTask> empty;
    std::swap(tasks_, empty);
}

size_t ThreadPool::PendingTasks() const {
    std::unique_lock<std::mutex> lock(queueMutex_);
    return tasks_.size();
}

void ThreadPool::WorkerLoop() {
    while (true) {
        PrioritizedTask task(TaskPriority::Normal, nullptr);
        
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            
            condition_.wait(lock, [this] {
                return !running_.load() || !tasks_.empty();
            });
            
            if (!running_.load() && tasks_.empty()) {
                return;
            }
            
            if (tasks_.empty()) {
                continue;
            }
            
            task = std::move(const_cast<PrioritizedTask&>(tasks_.top()));
            tasks_.pop();
        }
        
        if (task.task) {
            activeTasks_.fetch_add(1);
            
            try {
                task.task();
            } catch (...) {
                // Swallow exceptions from tasks
            }
            
            activeTasks_.fetch_sub(1);
            waitCondition_.notify_all();
        }
    }
}

// ============================================================================
// Global Thread Pool
// ============================================================================

namespace {
    std::unique_ptr<ThreadPool> g_globalPool;
    std::mutex g_globalPoolMutex;
    std::once_flag g_globalPoolInitFlag;
}

ThreadPool& GetGlobalThreadPool() {
    std::call_once(g_globalPoolInitFlag, []() {
        std::lock_guard<std::mutex> lock(g_globalPoolMutex);
        if (!g_globalPool) {
            g_globalPool = std::make_unique<ThreadPool>();
        }
    });
    
    std::lock_guard<std::mutex> lock(g_globalPoolMutex);
    return *g_globalPool;
}

void InitGlobalThreadPool(const ThreadPool::Config& config) {
    std::lock_guard<std::mutex> lock(g_globalPoolMutex);
    
    if (g_globalPool) {
        g_globalPool->Shutdown();
    }
    
    g_globalPool = std::make_unique<ThreadPool>(config);
}

void ShutdownGlobalThreadPool() {
    std::lock_guard<std::mutex> lock(g_globalPoolMutex);
    
    if (g_globalPool) {
        g_globalPool->Shutdown();
        g_globalPool.reset();
    }
}

// ============================================================================
// TaskGroup Implementation
// ============================================================================

TaskGroup::TaskGroup(ThreadPool& pool) : pool_(pool) {}

TaskGroup::~TaskGroup() {
    try {
        Wait();
    } catch (...) {
        // Suppress exceptions in destructor
    }
}

void TaskGroup::Wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] {
        return pendingCount_ == 0;
    });
    
    // Rethrow any captured exception
    if (hasException_) {
        std::exception_ptr ex = exception_;
        hasException_ = false;
        exception_ = nullptr;
        std::rethrow_exception(ex);
    }
}

bool TaskGroup::WaitFor(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    bool result = condition_.wait_for(lock, timeout, [this] {
        return pendingCount_ == 0;
    });
    
    if (result && hasException_) {
        std::exception_ptr ex = exception_;
        hasException_ = false;
        exception_ = nullptr;
        std::rethrow_exception(ex);
    }
    
    return result;
}

size_t TaskGroup::PendingCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pendingCount_;
}

bool TaskGroup::HasException() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hasException_;
}

void TaskGroup::RethrowException() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (hasException_) {
        std::exception_ptr ex = exception_;
        hasException_ = false;
        exception_ = nullptr;
        std::rethrow_exception(ex);
    }
}

// ============================================================================
// Scheduler Implementation
// ============================================================================

Scheduler::Scheduler() 
    : pool_(&GetGlobalThreadPool())
    , ownPool_(false) {}

Scheduler::Scheduler(ThreadPool& pool)
    : pool_(&pool)
    , ownPool_(false) {}

Scheduler::~Scheduler() {
    Stop();
}

void Scheduler::Start() {
    if (running_.exchange(true)) {
        return; // Already running
    }
    
    schedulerThread_ = std::thread(&Scheduler::SchedulerLoop, this);
}

void Scheduler::Stop() {
    if (!running_.exchange(false)) {
        return; // Not running
    }
    
    condition_.notify_all();
    
    if (schedulerThread_.joinable()) {
        schedulerThread_.join();
    }
    
    // Clear remaining tasks
    {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!tasks_.empty()) {
            tasks_.pop();
        }
    }
}

bool Scheduler::Cancel(uint64_t taskId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Note: We can't efficiently cancel a specific task in a priority queue
    // We mark it as cancelled and skip it during execution
    // This is a simplified implementation
    
    // Rebuild queue without the cancelled task
    std::vector<ScheduledTask> remaining;
    bool found = false;
    
    while (!tasks_.empty()) {
        auto task = std::move(const_cast<ScheduledTask&>(tasks_.top()));
        tasks_.pop();
        
        if (task.id == taskId) {
            found = true;
        } else {
            remaining.push_back(std::move(task));
        }
    }
    
    for (auto& task : remaining) {
        tasks_.push(std::move(task));
    }
    
    return found;
}

void Scheduler::CancelAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!tasks_.empty()) {
        tasks_.pop();
    }
}

size_t Scheduler::TaskCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

uint64_t Scheduler::ScheduleTask(std::chrono::steady_clock::time_point time,
                                  std::chrono::milliseconds period,
                                  std::function<void()> func) {
    uint64_t id = nextId_.fetch_add(1);
    
    ScheduledTask task;
    task.id = id;
    task.nextRun = time;
    task.period = period;
    task.task = std::move(func);
    task.cancelled = false;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push(std::move(task));
    }
    
    condition_.notify_one();
    return id;
}

void Scheduler::SchedulerLoop() {
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (tasks_.empty()) {
            // Wait for new tasks or stop signal
            condition_.wait(lock, [this] {
                return !running_.load() || !tasks_.empty();
            });
            continue;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto& nextTask = tasks_.top();
        
        if (nextTask.nextRun > now) {
            // Wait until task is due or new task arrives
            auto waitTime = nextTask.nextRun - now;
            condition_.wait_for(lock, waitTime);
            continue;
        }
        
        // Task is due - execute it
        ScheduledTask task = std::move(const_cast<ScheduledTask&>(tasks_.top()));
        tasks_.pop();
        
        if (task.cancelled) {
            continue;
        }
        
        // Execute task on thread pool
        pool_->Execute([taskFunc = task.task]() {
            try {
                taskFunc();
            } catch (...) {
                // Swallow exceptions
            }
        });
        
        // Reschedule if periodic
        if (task.period.count() > 0) {
            task.nextRun = std::chrono::steady_clock::now() + task.period;
            tasks_.push(std::move(task));
        }
    }
}

} // namespace util
} // namespace shurium
