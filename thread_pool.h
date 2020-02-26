#ifndef THREAD_POOL_H
#define THREAD_POOL_H

/*!
 * \file thread_pool.h
 * \brief A thread pool implementation.
 * \author Raphaël Lefèvre
*/

#include <queue>              // std::queue
#include <vector>             // std::vector
#include <thread>             // std::thread
#include <atomic>             // std::atomic
#include <mutex>              // std::mutex
#include <condition_variable> // std::condition_variable

/*!
 * \namespace concurrency
 * \brief Namespace encapsulating the thread pool implementation.
 */
namespace concurrency
{
    /*!
     * \brief Structure to define a task.
     *
     * Any task given to the thread pool must inherits the Task structure.
     */
    struct Task
    {
        virtual ~Task() = default;
        /*!
         * \brief The task to be executed.
         */
        virtual void run() = 0;
    };

    /*!
     * \brief A thread pool.
     */
    class ThreadPool final
    {
        private:
            const std::size_t threads_count_;

            std::queue<Task*> tasks_pool_;
            mutable std::mutex guardian_;

            std::vector<std::thread> threads_pool_;
            std::atomic<bool> alive_;

            std::atomic<std::size_t> running_tasks_count_;
        
            std::condition_variable thread_spin_cv_;

        public:
            /*!
             * \brief The policy to adopt when we stop the thread pool.
             */
            enum STOP_POLICY {SYNC /*!< Wait (block) until all threads have finished. */, ASYNC /*!< Delegate the threads synchronization (returns directly). See joinAll(). */};
            /*!
             * \brief Status to describe whether the thread pool is started or not.
             */
            enum STATUS {UP /*!< The thread pool is up (already started). */, DOWN /*! The thread pool is down (stopped or never started). <*/};

            /*!
             * \brief Constructor.
             * \param[in] threads_count The desired number of threads -- Default: `std::thread::hardware_concurrency()`
             */
            ThreadPool(std::size_t threads_count = std::thread::hardware_concurrency());
            ~ThreadPool();

            /*!
             * \brief Start the thread pool.
             *
             * Bring up the threads and start the spinning.
             */
            void start();
            /*!
             * \brief Stop the thread pool.
             * \param[in] sp The \ref ThreadPool::STOP_POLICY "stop policy"
             *
             * Stop the threads spinning and lead the threads to termination.
             */
            void stop(STOP_POLICY sp = SYNC);
            /*!
             * \brief Get the current status of the thread pool.
             * \return \ref ThreadPool::UP "UP" if already started, \ref ThreadPool::DOWN "DOWN" otherwise
             */
            STATUS status() const;
            /*!
             * \brief Synchronize the threads termination.
             *
             * This function can be called if the thread pool was \ref ThreadPool::ASYNC "asynchronously" \ref stop() "stopped".
             *
             * \note This function does nothing if the thread pool is still alive.
             */
            void joinAll();
            /*!
             * \brief Add a task to the tasks queue.
             * \param[in] t The task to add
             *
             * \note A task can be added at any time, whether the thread pool is up or not.
             */
            void addTask(Task * t);
            /*!
             * \brief Clear the tasks queue.
             *
             * All the pending tasks are removed and will not be executed.
             *
             * \note The running tasks (already handled by a thread) will not be affected. They will keep running until completion.
             */
            void clear();
            /*!
             * \brief Check if there are pending tasks.
             * \return `false` if the task queue is empty, `true` otherwise
             */
            bool is_pending() const;
            /*!
             * \brief Get the number of pending tasks.
             * \return The number of pending tasks
             */
            std::size_t pending() const;
            /*!
             * \brief Check if there are running tasks.
             * \return `true` is there is at least one running task, `false` otherwise
             */
            bool is_running() const;
            /*!
             * \brief Get the number of running tasks.
             * \return The number of running tasks
             */
            std::size_t running() const;
            /*!
             * \brief Wait (block) until all pending tasks are handled and finished.
             *
             * This function returns when there is no pending nor running tasks left (i.e. `(!is_pending() && !is_running()) == true`).
             */
            void waitForIdle() const;

        private:
            void spin(); // Each thread execute the spin() function with its own control block.
            void clean_up();
    };
}

#endif // THREAD_POOL_H
