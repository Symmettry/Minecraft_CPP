#pragma once

#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <tracy/Tracy.hpp>

class ThreadPool {
public:
    explicit ThreadPool(const size_t numThreads, const char* threadName = "Worker") : stop(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this, threadName] {
                tracy::SetThreadName(threadName);
                while (true) {
                    std::function<void()> job;

                    {
                        std::unique_lock lock(queueMutex);
                        condition.wait(lock, [this] { return stop || !jobs.empty(); });

                        if (stop && jobs.empty())
                            return;

                        job = std::move(jobs.front());
                        jobs.pop_front();
                        activeJobs.fetch_add(1, std::memory_order_relaxed);
                    }

                    {
                        ZoneScopedN("Worker job");
                        job();
                        activeJobs.fetch_sub(1, std::memory_order_relaxed);
                    }
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (auto &thread : workers) {
            if (thread.joinable()) thread.join();
        }
    }

    // priority jobs run before everything already queued
    void enqueue(std::function<void()> job, const bool priority = false) {
        {
            std::unique_lock lock(queueMutex);
            if (priority) jobs.push_front(std::move(job));
            else jobs.push_back(std::move(job));
        }
        condition.notify_one();
    }

    [[nodiscard]] size_t threadCount() const { return workers.size(); }
    [[nodiscard]] size_t activeJobCount() const { return activeJobs.load(std::memory_order_relaxed); }
    [[nodiscard]] size_t pendingJobCount() const {
        std::lock_guard lock(queueMutex);
        return jobs.size();
    }

private:
    std::vector<std::thread> workers;
    std::deque<std::function<void()>> jobs;

    mutable std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic_size_t activeJobs{0};
    bool stop;
};
