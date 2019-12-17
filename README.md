# ThreadPool
Thread pool C++ implementation

Copyright (c) 2019, Raphaël Lefèvre

### 1 - Overview
This is a C++ thread pool implementation based on the standard library requiring at least C++11.

- A `ThreadPool` object manages a pool of threads and a tasks queue.

- We can specify how many workers are required in the `ThreadPool` constructor. By default, it takes [`std::thread::hardware_concurrency()`](https://en.cppreference.com/w/cpp/thread/thread/hardware_concurrency) threads.

- The `ThreadPool` works on `Task` objects. Any task that has to be handled by the `ThreadPool` must derive the `Task` structure and override the `run()` method.

- Tasks can be added to the `ThreadPool` at anytime, whether it is started or not.

- When started, the `ThreadPool` creates the required number of threads and makes them spin. If there a pending task in the queue and a thread is available, the task will be assigned to this thread and removed from the queue.

- After being stopped, the `ThreadPool` can be safely restarted. If already started, trying to start again will have no effect.

- When the `ThreadPool` is stopped, the threads will stop to spin and be destroyed (after properly completing the current running tasks if any). The pending tasks in the queue will not be executed (except if the `ThreadPool` is restarted and the queue has not been emptied in the meanwhile).

- We can specify a stop policy when stopping a `ThreadPool`:
    - `SYNC` (synchronous) : Send the termination command and wait until all the workers are terminated (blocking). This is the default value.
    - `ASYNC` (asynchronous) : Send the termination command and directly returns (non-blocking), delegating the responsibility of the threads termination synchronization to the user (in this case, `ThreadPool::joinAll()` must be called when the running tasks are required to be completed).

- If a `ThreadPool` is asynchronously stopped and then restarted (without previously calling `joinAll()`) while there are still running tasks, the threads termination synchronization will automatically be performed before the restart as if `joinAll()` was called.

_A Doxygen documentation is provided and can be generated for more information about the full available features_

### 2 - Basic usage examples


#### Use case 1:
Considering the following task:
```cpp
//.hpp
struct MyTask : public concurrency::Task
{
    void run() override;
};

//.cpp
void MyTask::run()
{
    // Do something
    std::this_thread::sleep_for(std::chrono::seconds(5));
}
```

We want to execute 30 tasks with a threads pool of 10 threads:
```cpp
// Create 30 tasks
std::vector<MyTask> tasks_to_execute;
for(std::size_t i = 0; i < 30; ++i)
    tasks_to_execute.push_back(MyTask{});

// Create a thread pool of 10 threads
concurrency::ThreadPool tp(10);

// Add the tasks to the thread pool
for(auto & t : tasks_to_execute)
    tp.addTask(&t);

// Start the thread pool (create the 10 threads and make them spin)
tp.start();
std::cout << "Thread pool started !" << std::endl; // Important to flush the buffer

// Wait until all tasks are fully completed
tp.waitForIdle();

// Stop the thread pool (stop the threads spinning and destroy the threads).
// Note: The thread pool can be restarted after being stopped.
tp.stop();
std::cout << "Thread pool stopped !" << std::endl;
```

#### Use case 2
Now let's consider we have 3 tasks `t1`, `t2` and `t3` to execute. And `t3` must wait that `t2` has finished before being started.

We can define a task that will notify when its execution is completed:
```cpp
//.h
struct MyOtherTask : public concurrency::Task
{
    private:
        std::atomic<bool> finished;

    public:
        MyOtherTask();
        bool is_finished() const;

        void run() override;
};

//.cpp
MyOtherTask::MyOtherTask() : finished(false)
{}
bool MyOtherTask::is_finished() const
{
    return finished;
}
void MyOtherTask::run()
{
    finished = false; // in the case the task is reran (ran again)

    // Do something
    std::this_thread::sleep_for(std::chrono::seconds(5));

    finished = true;
}
```
Then we can apply the precedence between `t2` and `t3` as follows:
```cpp
// Create 3 tasks
MyTask t1;
MyOtherTask t2; // is able to notify when it finished
MyTask t3;

// Create a thread pool
concurrency::ThreadPool tp;

// Add the tasks
tp.addTask(&t1);
tp.addTask(&t2);

// Start the threads spinning
tp.start();

// Wait until t2 has finished
std::cout << std::boolalpha;
std::cout << "t2 finished ? " << t2.is_finished() << "\nWaiting..." << std::endl;

while(!t2.is_finished())
    std::this_thread::yield();

std::cout << "t2 finished ? " << t2.is_finished() << "\nRun t3..." << std::endl;

// Run the t3 when t2 is finished
// Note: We can add tasks on-the-fly (when the thread pool is already started)
tp.addTask(&t3);

// Wait for idle
tp.waitForIdle();

// Stop
tp.stop();
```
