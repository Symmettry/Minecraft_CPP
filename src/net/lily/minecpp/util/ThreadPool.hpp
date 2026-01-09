#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool {
public:
    explicit ThreadPool(const size_t numThreads) : stop(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> job;

                    {
                        std::unique_lock lock(queueMutex);
                        condition.wait(lock, [this] { return stop || !jobs.empty(); });

                        if (stop && jobs.empty())
                            return;

                        job = std::move(jobs.front());
                        jobs.pop();
                    }

                    job();
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

    void enqueue(std::function<void()> job) {
        {
            std::unique_lock lock(queueMutex);
            jobs.push(std::move(job));
        }
        condition.notify_one();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> jobs;

    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};
