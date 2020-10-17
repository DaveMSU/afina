#include <afina/concurrency/Executor.h>

namespace Afina {
namespace Concurrency {

void perform( Executor* executor, size_t th_idx ){

	while( executor->state == Executor::State::kRun ){ // Выполняем пока это нужно.

		// Проверяем можно ли брать из очереди, если да, то
		//  берем на исполнение задачу и исполняем, затем на while
		//  если нет, то ждем idle_time, если за это время задача появилась, то
		//  берем на исполнение задачу и исполняем, если нет
		//  убиваем поток - удяляем thread объект из вектора и выходим из функции.
		// 
		if( empty_condition.wait_for(mutex, 
					     std::chrono::milliseconds(executor->idle_time), 
					     [](){ return !executor->tasks.empty(); }) ){
		
			{
				// Чтобы никто другой не взял эту задачу.
				//
				std::unique_lock lock(mutex);
				std::function<Execute> task = executor->tasks.front();
				executor->tasks.pop_front();
			}

			task();			
		}
		else{
			// Проверяем, достигли ли мы минимума, если да - создаем новый поток.
			//
			if( executor->threads.size() == executor->low_watermark ){

				size_t idx = executor->threads.size();
				executor->threads.emplace_back(
						std::thread([](){ return perform(executor, idx)}) );
			}

			executor->threads.erase(th_idx);
		}
	}
}

}
} // namespace Afina
