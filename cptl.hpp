/*********************************************************
 *
 *  Copyright (C) 2014 by Vitaliy Vitsentiy
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *********************************************************/

#ifndef __ctpl_thread_pool_H__
#define __ctpl_thread_pool_H__

#include <atomic>
#include <boost/lockfree/queue.hpp>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#ifndef _ctplThreadPoolLength_
#define _ctplThreadPoolLength_ 100
#endif

// thread pool to run user's functors with signature
//      ret func(int id, other_params)
// where id is the index of the thread that runs the functor
// ret is some return type

namespace ctpl {

class thread_pool {
public:
    thread_pool() : q(_ctplThreadPoolLength_) { this->init(); }
    thread_pool(int nThreads, int queueSize = _ctplThreadPoolLength_)
        : q(queueSize)
    {
        this->threads.resize(nThreads);
        for (int i = oldNThreads; i < nThreads; ++i) {
            this->set_thread(i);
        }
    }

    // the destructor waits for all the functions in the queue to be finished
    ~thread_pool() { this->stop(); }

    // get the number of running threads in the pool
    int size() { return static_cast<int>(this->threads.size()); }

    // number of idle threads
    int n_idle() { return this->nWaiting; }
    std::thread &get_thread(int i) { return *this->threads[i]; }

    // empty the queue
    void clear_queue()
    {
        std::function<void(int id)> *_f;
        while (this->q.pop(_f))
            delete _f;  // empty the queue
    }

    // pops a functional wraper to the original function
    std::function<void(int)> pop()
    {
        std::function<void(int id)> *_f = nullptr;
        this->q.pop(_f);
        std::unique_ptr<std::function<void(int id)>> func(
            _f);  // at return, delete the function even if an exception
                  // occurred

        std::function<void(int)> f;
        if (_f)
            f = *_f;
        return f;
    }

    // wait for all computing threads to finish and stop all threads
    // may be called asyncronously to not pause the calling thread while waiting
    // All the functions in the queue are run
    void stop()
    {
        if (this->isDone)
            return;
        this->isDone = true;  // give the waiting threads a command to finish
        {
            std::unique_lock<std::mutex> lock(this->mutex);
            this->cv.notify_all();  // stop all waiting threads
        }
        for (int i = 0; i < static_cast<int>(this->threads.size());
             ++i) {  // wait for the computing threads to finish
            if (this->threads[i]->joinable())
                this->threads[i]->join();
        }
        // if there were no threads in the pool but some functors in the queue,
        // the functors are not deleted by the threads therefore delete them
        // here
        this->clear_queue();
        this->threads.clear();
    }

    // run the user's function that excepts argument int - id of the running
    // thread. returned value is templatized operator returns std::future, where
    // the user can get the result and rethrow the catched exceptins
    template <typename F>
    auto push(F &&f) -> std::future<decltype(f(0))>
    {
        auto pck = std::make_shared<std::packaged_task<decltype(f(0))(int)>>(
            std::forward<F>(f));

        auto _f =
            new std::function<void(int id)>([pck](int id) { (*pck)(id); });
        this->q.push(_f);

        std::unique_lock<std::mutex> lock(this->mutex);
        this->cv.notify_one();

        return pck->get_future();
    }

private:
    // deleted
    thread_pool(const thread_pool &);             // = delete;
    thread_pool(thread_pool &&);                  // = delete;
    thread_pool &operator=(const thread_pool &);  // = delete;
    thread_pool &operator=(thread_pool &&);       // = delete;

    void set_thread(int i)
    {
        auto f = [this, i]() {
            std::function<void(int id)> *_f;
            bool isPop = this->q.pop(_f);
            while (true) {
                while (isPop) {  // if there is anything in the queue
                    std::unique_ptr<std::function<void(int id)>> func(
                        _f);  // at return, delete the function even if an
                              // exception occurred
                    (*_f)(i);
                    isPop = this->q.pop(_f);
                }

                // the queue is empty here, wait for the next command
                std::unique_lock<std::mutex> lock(this->mutex);
                ++this->nWaiting;
                this->cv.wait(lock, [this, &_f, &isPop]() {
                    isPop = this->q.pop(_f);
                    return isPop || this->isDone;
                });
                --this->nWaiting;

                if (!isPop)
                    return;  // if the queue is empty and this->isDone == true
                             // or *flag then return
            }
        };
        this->threads[i].reset(
            new std::thread(f));  // compiler may not support std::make_unique()
    }

    std::vector<std::unique_ptr<std::thread>> threads;
    mutable boost::lockfree::queue<std::function<void(int id)> *> q;
    std::atomic<bool> isDone = false;
    std::atomic<int> nWaiting = 0;  // how many threads are waiting

    std::mutex mutex;
    std::condition_variable cv;
};

}  // namespace ctpl

#endif  // __ctpl_thread_pool_H__