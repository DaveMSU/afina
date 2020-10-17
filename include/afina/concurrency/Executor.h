#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace Afina {
namespace Concurrency {

/**
 * # Thread pool
 */
class Executor {
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };

    Executor( std::string name,
              size_t size, 
	      size_t low_wmark = 4, 
	      size_t hight_wmark = 16,
	      size_t wait_time = 100) : low_watermark(low_wmark),
	                                hight_watermark(hight_wmark),
	                                max_queue_size(size),
				        idle_time(wait_time){

    	for( size_t i = 0; i < max_queue_size; ++i ){

		threads.push_back( std::thread([](){ return perform(this, i) }) );
	} 

    }

    ~Executor(){}

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false){

	state = State::kStopping;

	for( size_t i = 0; i < threads.size(); ++i ){

		threads[i].join();
	}

	state = State::kStopped;
    }


    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> lock(this->mutex);
        if (state != State::kRun) {
            return false;
        }

        // Enqueue new task
        tasks.push_back(exec);
        empty_condition.notify_one();
        return true;
    }

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    friend void perform( Executor *executor, size_t th_idx );

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;
    std::timed_mutex mutex_time;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

    /**
     * Vector of actual threads that perorm execution
     */
    std::vector<std::thread> threads;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    State state;

    /**
     * Descriptions... tra-ta-ta...
     */
    size_t low_watermark;
    size_t hight_watermakr;
    size_t max_queue_size;
    size_t idle_time;
    using Ms = std::chrono::milliseconds;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
