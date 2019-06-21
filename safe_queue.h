//
// Created by 牛天睿 on 17/3/19.
//

#ifndef KERNEL_SAFE_QUEUE_H
#define KERNEL_SAFE_QUEUE_H
#include "functor.hpp"
#include <atomic>
#include <condition_variable>
namespace tp {

    class safe_queue {
        using data=functor;
    public:
        safe_queue() {}

        safe_queue(const safe_queue &other) = delete;

        safe_queue &operator=(const safe_queue &other)= delete;

        virtual bool emplace(data &&d) = 0;
        virtual bool try_pop(data &d) = 0;
    };

}




#endif //KERNEL_SAFE_QUEUE_H
