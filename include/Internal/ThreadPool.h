#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <variant>
#include <vector>

namespace SL {
namespace NET {
    template <class RWContext, class ConntextContext, class ProcessItemCB> class ThreadPool {

        using var_t = std::variant<RWContext, ConntextContext>;
        ProcessItemCB ProcessItemFunc;

      public:
        ThreadPool(size_t threads, ProcessItemCB &)> &f) : stop(false), ProcessItemFunc(f)
        {
            for (size_t i = 0; i < threads; ++i)
                workers.emplace_back([this] {
                    for (;;) {
                        var_t task;

                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                            if (this->stop && this->tasks.empty())
                                return;
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }
                        ProcessItemFunc(task);
                    }
                });
        }
        ~ThreadPool()
        {
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                stop = true;
            }
            condition.notify_all();
            for (std::thread &worker : workers)
                worker.join();
        }

      private:
        // need to keep track of threads so we can join them
        std::vector<std::thread> workers;
        // the task queue
        std::queue<var_t> tasks;

        // synchronization
        std::mutex queue_mutex;
        std::condition_variable condition;
        bool stop;
        void enqueue(var_t &&f)
        {
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                // don't allow enqueueing after stopping the pool
                if (stop)
                    throw std::runtime_error("enqueue on stopped ThreadPool");
                tasks.emplace(std::move(f));
            }
            condition.notify_one();
        }
    };
} // namespace NET
} // namespace SL
