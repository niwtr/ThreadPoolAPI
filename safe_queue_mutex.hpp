#ifndef safe_queue_mutex_h
#define safe_queue_mutex_h
#include <deque>
#include <mutex>
#include "functor.hpp"
#include "safe_queue.h"
#include <atomic>
namespace tp {

    class safe_queue_mutex : public safe_queue {
    private:
        using data = functor;
        std::deque <data> the_queue;
        mutable std::mutex the_mutex;

        static std::atomic_int count;

    public:
        safe_queue_mutex() {}

        safe_queue_mutex(const safe_queue &other) = delete;

        safe_queue_mutex &operator=(const safe_queue_mutex &other)= delete;

        virtual bool emplace(data &&d) override {
            std::lock_guard <std::mutex> lock(the_mutex);
            the_queue.emplace_front(std::forward<data>(d));
            return true;
        }



        bool empty() const {
            std::lock_guard <std::mutex> lock(the_mutex);
            return the_queue.empty();
        }

        virtual bool try_pop(data &d) override {
            std::lock_guard <std::mutex> lock(the_mutex);
            if (the_queue.empty()) {
                return false;
            }
            d = std::move(the_queue.front());
            the_queue.pop_front();
            return true;
        }


    };

}

#endif /* safe_queue */