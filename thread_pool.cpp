#include "thread_pool.h"

namespace concurrency
{
    ThreadPool::ThreadPool(std::size_t threads_count) : threads_count_(threads_count), alive_(false), running_tasks_count_(0)
    {}
    ThreadPool::~ThreadPool()
    {
        this->stop(SYNC);
    }

    void ThreadPool::start()
    {
        // Do nothing if already alive.
        if(!alive_)
        {
            // Empty the threads pool if it is not
            // i.e. stop() was called with STOP_POLICY::ASYNC.
            if(!threads_pool_.empty())
            {
                this->clean_up();
            }

            // Start
            alive_ = true;
            for(std::size_t i = 0; i < threads_count_; ++i)
                threads_pool_.push_back(std::thread(&ThreadPool::spin, this));
        }
    }
    void ThreadPool::stop(STOP_POLICY sp)
    {
        {
            std::lock_guard<std::mutex> locker(guardian_); // Lock to make the condition_variable's predicate evaluation safe (let it finish and return before the wait() releases the mutex -> Prevent threads to miss the notify_all()).
            alive_ = false;
        }
        thread_spin_cv_.notify_all();

        if(sp == SYNC && !threads_pool_.empty())
        {
            this->clean_up();
        }
    }
    ThreadPool::STATUS ThreadPool::status() const
    {
        return (alive_ ? UP : DOWN);
    }
    void ThreadPool::joinAll()
    {
        if(!alive_ && !threads_pool_.empty())
        {
            this->clean_up();
        }
    }
    void ThreadPool::addTask(Task * t)
    {
        std::lock_guard<std::mutex> locker(guardian_);
        tasks_pool_.push(t);
        if(alive_)
            thread_spin_cv_.notify_one();
    }
    void ThreadPool::clear()
    {
        std::queue<Task*> tmp;
        guardian_.lock();
        tasks_pool_.swap(tmp);
        guardian_.unlock();
    }
    bool ThreadPool::is_pending() const
    {
        std::lock_guard<std::mutex> locker(guardian_);
        return !tasks_pool_.empty();
    }
    std::size_t ThreadPool::pending() const
    {
        std::lock_guard<std::mutex> locker(guardian_);
        return tasks_pool_.size();
    }
    bool ThreadPool::is_running() const
    {
        return running_tasks_count_;
    }
    std::size_t ThreadPool::running() const
    {
        return running_tasks_count_;
    }
    void ThreadPool::waitForIdle() const
    {
        while(is_pending() || is_running())
            std::this_thread::yield();
    }

    void ThreadPool::spin()
    {
        Task * task;
        while(alive_)
        {
            std::unique_lock<std::mutex> lk(guardian_);
            thread_spin_cv_.wait(lk, [&](){return !alive_ || !tasks_pool_.empty();});
            
            // At this point, if "alive_ == true" then tasks_pool_ cannot be empty (the lock was reacquired before evaluating the predicate).

            if(alive_)
            {
                task = tasks_pool_.front();
                tasks_pool_.pop();
                lk.unlock();

                ++running_tasks_count_;
                task->run();
                --running_tasks_count_;
            }
            // No need to "else lk.unlock()" here since the std::unique_lock will already release the mutex at destruction.
        }
    }
    void ThreadPool::clean_up()
    {
        for(std::thread & th : threads_pool_)
        {
            if(th.joinable())
                th.join();
        }
        threads_pool_.clear();
    }
}
