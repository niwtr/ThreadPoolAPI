# ThreadPoolAPI: A simple yet efficient Thread Pool API written in C++14
## Requirements
Requires a C++-14 compiler.
## Usage
This thread pool API at its internal is implemented as a queue. So first you need to grab a specific type of queue. 
The code ships with three types of queue, namely: 
* `MPMCBoundedQueue` : A thread-safe, multi-producer/multi-consumer lock-free queue. This one always has the best performance. So pick this up if you have no idea about them. 
* `safe_queue_mutex` : A thread-safe queue implemented using Mutex lock. 
* `safe_queue_condvar` : A thread-safe queue implemented using Conditional Variable. 

Then, initialize a ThreadPool.
```C++
//Create a smart pointer of this ThreadPool object 
//Initilaize the thread pool with 1000 threads.
auto tpool = std::make_shared<tp::ThreadPool_noblock<tp::MPMCBoundedQueue>>(1000);
tpool->init();
```

Next, commit your tasks by `tpool->commit()`
```C++
// commit a task without any argument.\
// you can perform this by using a lambda to capture the function argument manually.
tpool->commit_noarg([arg] {
	            do_some_thing(arg);
});
// commit a task with a two arguments.
int arg1 = 1, arg2 = 11;
tpool->commit_noarg([](int _arg1, int _arg2) {
	            do_some_thing2(_arg1, _arg2);
}, arg1, arg2);
```

