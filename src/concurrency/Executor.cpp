#include <afina/concurrency/Executor.h>

namespace Afina {
namespace Concurrency {

void perform( Executor* executor ){

	while( executor->state == Executor::State::kRun ){ // Выполняем пока это нужно.

		// Проверяем можно ли брать из очереди, если да, то
		//  берем на исполнение задачу и исполняем, затем возвращаемся на while,
		//  если нет, то ждем idle_time, если за это время задача появилась, то
		//  берем на исполнение задачу и исполняем, если нет
		//  убиваем поток - выходим из функции.
		//
		std::unique_lock<std::mutex> lock(executor->mutex);

		if( executor->empty_condition.wait_for(lock,
			           		       std::chrono::milliseconds(executor->idle_time), 
						       [executor](){ return !executor->tasks.empty(); }) ){
		
			std::function<void()> task;
			{
				// Чтобы никто другой не взял эту задачу.
				//
				std::unique_lock<std::mutex> lock(executor->mutex);
			        task = executor->tasks.front();
				executor->tasks.pop_front();
			}
			
			try {
				task();			
			}
			catch(...){
				
				throw std::runtime_error("exec command from queue was incorrectly runned!");
			}
		}
		else{
			
			// Поток умирает, т.к. не дождался задачи.
			// Уменьшаем счетчик числа потоков.
			//
			--executor->curr_watermark;

			if( executor->curr_watermark == 0 )
				executor->no_workers_condition.notify_one();

			return;
		}
	}

	// Сюда можем дойти, в случае, если в функции Stop, переменная state 
	//  перестанет быть равна kRun, то есть когда придет время всем 
	//  потокам завершить работу.
	//
	--executor->curr_watermark;
	if( executor->curr_watermark == 0 ){

		executor->no_workers_condition.notify_one();
		executor->state = executor->State::kStopped;
	}
}

}
} // namespace Afina
