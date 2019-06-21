//
// Created by 牛天睿 on 17/3/17.
//

#ifndef SERVER_CORE_BLOCKING_QUEUE_H
#define SERVER_CORE_BLOCKING_QUEUE_H
#include "safe_queue.h"
#include <deque>
#include <mutex>
#include "functor.hpp"
#include <atomic>
#include <condition_variable>

namespace tp {

    class blocking_queue : public safe_queue {
    private:
        using data = functor;
        std::deque <data> the_queue;
        mutable std::mutex the_mutex;
        std::condition_variable condvar;

        static std::atomic_int count;

    public:
        blocking_queue() {}

        blocking_queue(const blocking_queue &other) = delete;

        blocking_queue &operator=(const blocking_queue &other)= delete;

        virtual bool emplace(data &&d) override {
            std::lock_guard <std::mutex> lock(the_mutex);
            the_queue.emplace_front(std::forward<data>(d));
            condvar.notify_one();return true;
        }


        bool empty() const {
            return the_queue.empty();
        }

        virtual bool try_pop(data &d) override {
            std::unique_lock<std::mutex> lock {the_mutex};

            condvar.wait(lock, [this]{return not the_queue.empty();});
            d = std::move(the_queue.front());
            the_queue.pop_front();
            return true;
        }


    };

}



#endif //SERVER_CORE_BLOCKING_QUEUE_H
