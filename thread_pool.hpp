/**
Copyright (c) 2012 Jakob Progsch, Václav Zeman
This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.


Original sources:
https://github.com/progschj/ThreadPool
https://github.com/aphenriques/thread
 */
#pragma once

#ifndef thread_Pool_hpp
#define thread_Pool_hpp

#include <functional>
#include <future>
#include <queue>

namespace thread {
class Pool {
public:
    Pool(std::size_t numberOfThreads);

    // joins all threads
    ~Pool();

    template <class F, class... A>
    decltype(auto) enqueue(F &&callable, A &&... arguments);

private:
    std::vector<std::thread> threads_;
    std::queue<std::packaged_task<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool stopped_;
};

template <class F, class... A>
decltype(auto) Pool::enqueue(F &&callable, A &&... arguments)
{
    using ReturnType = std::invoke_result_t<F, A...>;
    std::packaged_task<ReturnType()> task(
        std::bind(std::forward<F>(callable), std::forward<A>(arguments)...));
    std::future<ReturnType> taskFuture = task.get_future();
    {
        std::unique_lock<std::mutex> lock(mutex_);
        tasks_.emplace(std::move(task));
        // attention! task moved
    }
    condition_.notify_one();
    return taskFuture;
}

Pool::Pool(std::size_t numberOfThreads) : stopped_(false)
{
    threads_.reserve(numberOfThreads);
    for (std::size_t i = 0; i < numberOfThreads; ++i) {
        threads_.emplace_back([this] {
            while (true) {
                std::packaged_task<void()> task;
                {
                    std::unique_lock<std::mutex> uniqueLock(mutex_);
                    condition_.wait(uniqueLock, [this] {
                        return tasks_.empty() == false || stopped_ == true;
                    });
                    if (tasks_.empty() == false) {
                        task = std::move(tasks_.front());
                        // attention! tasks_.front() moved
                        tasks_.pop();
                    }
                    else {  // stopped_ == true (necessarily)
                        return;
                    }
                }
                task();
            }
        });
    }
}

Pool::~Pool()
{
    {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        stopped_ = true;
    }
    condition_.notify_all();
    for (std::thread &worker : threads_) {
        worker.join();
    }
}

class Executor {
public:
    Executor(std::size_t numberOfThreads);

    template <class F, class... A>
    void enqueue(F &&callable, A &&... arguments);

    // waits completion of each enqueued task
    // other tasks may be enqueued after calling this method
    void join();

private:
    Pool pool_;
    std::queue<std::future<void>> futures_;
};

template <class F, class... A>
void Executor::enqueue(F &&callable, A &&... arguments)
{
    futures_.emplace(pool_.enqueue(std::bind<void>(
        std::forward<F>(callable), std::forward<A>(arguments)...)));
}

Executor::Executor(std::size_t numberOfThreads) : pool_(numberOfThreads) {}

void Executor::join()
{
    while (futures_.empty() == false) {
        futures_.front().get();
        futures_.pop();
    }
}

}  // namespace thread

#endif