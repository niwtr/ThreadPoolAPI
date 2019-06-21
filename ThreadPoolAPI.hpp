//
//  ThreadPoolAPI.h
//  ThreadPoolAPI
//
//  Created by 牛天睿 on 17/3/6.
//  Copyright © 2017年 牛天睿. All rights reserved.
//

#ifndef ThreadPoolAPI_h
#define ThreadPoolAPI_h
#pragma once
#include <iostream>
#include <thread>
#include <future>
#include <functional>
#include <utility>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

#include "safe_queue_mutex.hpp"
#include "safe_queue_condvar.hpp"
#include "mpmc_bounded_queue.hpp"
#define loop_count(x, n)\
for(int x=0;x<n;++x)

#define for_in(x, c)\
for(auto & x : c)

namespace tp {


    template<typename QT> /* queue type */
    class ThreadPool {
        /* a Task is a lambda or function that is wrapped in a functor class
         * that belongs to the tp namespace.
         * we have to warp up because we cannot use the original std::function
         * since it requires its object to be copy-constructable but
         * std::packaged_task is only movable. */
        using Task=tp::functor;
        using th_container = std::vector<std::thread>;

    protected:
        /* default num of threads.*/
        int num_of_threads = std::thread::hardware_concurrency();

        /* for task execution, commit task to this queue. */
        std::shared_ptr<QT> task_queue ;

        /* actual thread pool. */
        std::shared_ptr<th_container> pool;

        /* control flag for workers. if this one is set true each workers
         * will stop retrieving new tasks to run.
         * separate flags are needed for different pools. we may create new
         * pools when current pool is full using the "commit_instant" functions.*/
        std::shared_ptr<bool> shut_down ;
        /* atomic counter that is helpful to the "commit_instant" functions.
         * if this one reached num_of_threads a new pool will be created
         * and the old pool will be destroyed automatically when all threads
         * have done their work. this method is important for creating an illusion
         * of indefinite number of threads. */
        std::atomic_int num_unfinished_tasks;

    public:

        /* by default we create std::thread::hardware_concurrency() threads. */
        ThreadPool() {}

        /* create thread with num_of_threads threads */
        ThreadPool(int num_of_workers)
        {
            this->num_of_threads=num_of_workers;
        }

        /* sorry but no copy constructor. */
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool(const ThreadPool&&) = delete;


        ~ThreadPool(){
            if (nullptr != this->shut_down)
                *(this->shut_down) = true;
            if (nullptr != this->pool)
                for_in(th, *pool)
                   th.detach();

        }

        /* ok, init the thread pool with actual threads.
         * in default we create #CPU threads which is
         * the concurrency of your machine. */
        void init() {
            this->num_unfinished_tasks.store(0);
            this->pool=std::make_shared<th_container >();
            this->shut_down=std::make_shared<bool>(false);
            this->task_queue = std::make_shared<blocking_queue>();
            add_workers();
        }

        /* commit a task to the task queue
         * the task is a function and is able to take any number of arguments.
         * technically we perform currying to the arguments,
         * generating a lambda with no arguments for a task execution. */
        template <typename Func, typename ... Args>
        auto commit (Func && fn, Args && ...args){
            ensure_state();
            using ret_type =decltype(fn(args...));
            /* currying the function. */
            std::packaged_task<ret_type()> task(
                    [fn, args...]{return fn(args...);});
            std::future<ret_type >_result {task.get_future()};
            /* fine, the packaged task is then *moved* into the task_queue */
            /* which is constructed into a tp::functor. */
            task_queue->emplace(std::move(task));

            this->num_unfinished_tasks ++ ;
            return _result;
        }

        /* commit a task with no argument. it is not curryed for optimization.
         * for fast function execution, use this. */
        template <typename Func>
        auto commit_noarg(Func && fn){
            ensure_state();
            using ret_type =decltype(fn());
            std::packaged_task<ret_type()> task(std::move(fn));
            std::future<ret_type >_result {task.get_future()};//为什么这一句放到下一句之后会崩溃？
            task_queue->emplace(std::move(task));
            this->num_unfinished_tasks ++;
            return _result;
        }

    protected:
        /* ensure that thread pool is initialized and is running. */
        void ensure_state () throw(std::runtime_error){
            if (this->task_queue == nullptr or this->pool == nullptr)
                throw std::runtime_error("Error: Thread pool uninitialized.");
            if (*(this->shut_down) == true)
                throw std::runtime_error("Error: Thread pool has been shut down.");
        }
        /* add workers to fill in the thread pool.
         * by default we create std::thread::hardware_concurrency() threads. */
        void add_workers (){
            loop_count(_, num_of_threads){
                pool->emplace_back(
                        [this]{
                            while(not *(this->shut_down)){
                                Task tsk;
                                if (task_queue->try_pop(tsk)) {
                                    tsk();
                                    this->num_unfinished_tasks --;
                                }
                                else
                                    std::this_thread::yield();
                            }
                        });}}
    };
    template<> class ThreadPool<MPMCBoundedQueue> {
        /* use a blocking queue instead. */
        ThreadPool(...) = delete;
    };
    template<> class ThreadPool<safe_queue_mutex> {
        /* use a blocking queue instead. */
        ThreadPool(...) = delete;
    };



    template <typename QT> /* Queue Type */
    class ThreadPool_noblock
            : public ThreadPool<blocking_queue>{
    private:
        /* for task execution, commit task to this queue. */
        /* the task queue is thread-safe
         * so no lock or mutex is in need here. */
        std::shared_ptr<QT> task_queue;
    public:
        /* by default we create std::thread::hardware_concurrency() threads. */
        ThreadPool_noblock() {}

        /* create thread with num_of_threads threads */
        ThreadPool_noblock(int num_of_workers)
        {
            this->num_of_threads=num_of_workers;
        }

        /* sorry but no copy constructor. */
        ThreadPool_noblock(const ThreadPool_noblock&) = delete;

        /* commit an instant task.
         * the task is curried for translation to noarg functions
         * so double-call is performed when executing the task.
         * if current thread pool is full or else *about to* full,
         * a new pool is inited and is used for the execution of our new task. */
        template <typename Func, typename ...Args>
        auto commit_instant(Func && fn, Args && ...args){
            if(this->num_unfinished_tasks.load() >= this->num_of_threads)
                this->guarded_detach();
            return commit(std::forward<Func>(fn),
                          std::forward<Args>(args)...);
        }

        /* commit an instant task with no args (of course no currying or double call)
        * if current thread pool is full or else *about to* full,
        * a new pool is inited and is used for the execution of our new task. */
        template <typename Func>
        auto commit_instant_noarg(Func && fn){

            if(this->num_unfinished_tasks.load() >= this->num_of_threads)
                this->guarded_detach();
            return commit_noarg(std::forward<Func>(fn));
        }


    private:
        /* in order to create a new pool for storage of more threads,
         * we need to detach old states and supplant it with a brand new one.
         * we have this done by calling init().
         * we need also guard the old threads to quit safely after their job is done.
         * this work is done by creating a new thread, or a daemon to handle them. */
        void guarded_detach (){
            /* launch a new thread to handle the old threads.
             * this is done asynchronously for efficiency. */
            using ptr_type=decltype(this->pool);
            std::thread([&](ptr_type last_pool) {
                for_in(th, *last_pool)
                    if (th.joinable())
                        th.join();
                last_pool->clear();
            }, this->pool).detach();
            *(this->shut_down) = true;
            this->init();
        }
    };

    template <> class ThreadPool_noblock<tp::blocking_queue>
            :public ThreadPool<blocking_queue>{
        /* use a non-blocking version queue type instead.*/
        ThreadPool_noblock(...)= delete;
    };



}




#endif /* ThreadPoolAPI_h */
