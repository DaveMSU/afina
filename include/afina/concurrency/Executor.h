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
	
	curr_watermark = 0;
	std::unique_lock<std::mutex> lock(mutex);
    	for( size_t i = 0; i < low_watermark; ++i ){

		std::thread thread([this](){ return perform(this); });
		++curr_watermark;
		thread.detach(); // Чтобы обеспечить безвредное завершение работы потоком при false в Stop.
	} 
    }

    ~Executor(){
    
	if( state != State::kRun ){

		Stop(true);		    
	}
	else{
	
		throw std::runtime_error("Executor already stopping or stopped!");
	}
    }

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop( bool await = false ){

	// Теперь все потоки, выполняющие функцию perform
	//  придя на while, закончат работу.
	//
	state = State::kStopping;

	// Если True, то дожидаемся пока они все не завершат работу,
	//  иначе, просто выходим из функции, доверяя потокам, что 
	//  такая асинхронная работа ничему не навредит.
	//
	if( await ){

		while( curr_watermark > 0 ){ ;
		
			// Нужен ли тут cv? 
			// Мне кажется, что нет.
			// Единственное, что смущает
			//  мб зацикливание будет
			//  затратнее, чем ожидание на cv.
		}
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
	if( tasks.size() == max_queue_size ){

		return false;
	}
        tasks.push_back(exec);
        empty_condition.notify_all(); // Иначе при '..._one()' выродится в однопоточную.
	while( curr_watermark < hight_watermark && !tasks.empty() ){
		
		std::thread thread([this](){ return perform(this); });
		++curr_watermark;
		thread.detach();
	}

	// Увеличиться кол-во задач, может только в этой функции.
	//  поэтому не нужна синхранизация mutex'ами.
	//
	if( !tasks.empty() ){

		return false;
	}

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
    friend void perform( Executor *executor );

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

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
    size_t curr_watermark;
    size_t low_watermark;
    size_t hight_watermark;
    size_t max_queue_size;
    size_t idle_time;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
