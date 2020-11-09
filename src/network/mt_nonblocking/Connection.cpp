#include "Connection.h"
#include <sys/socket.h>
#include <unistd.h>
#include <sys/uio.h>
#include <atomic>

#include <iostream>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start(){ 

	_event.events |= EPOLLIN | EPOLLERR | EPOLLHUP;

	_logger->debug("Connection started with {} event", _socket);
}

// See Connection.h
void Connection::OnError(){ 

	_logger->error("Error on current connection");
}

// See Connection.h
void Connection::OnClose(){

        close(_socket);

 	_logger->debug("Conncetion closed");	
}

// See Connection.h
void Connection::DoRead(){

	std::atomic_thread_fence(std::memory_order_acquire); // Чистим кэш.
	try {
    		int readed_bytes = -1;
    		char client_buffer[4096];
    		while ((readed_bytes = read(_socket, client_buffer, sizeof(client_buffer))) > 0) {
			_logger->debug("Got {} bytes from socket", readed_bytes);

			// Single block of data readed from the socket could trigger inside actions a multiple times,
			// for example:
	                // - read#0: [<command1 start>]
        	        // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
	                while (readed_bytes > 0){

    				_logger->debug("Process {} bytes", readed_bytes);
    	                // There is no command yet
			if (!_command_to_execute) {

				std::size_t parsed = 0;
				if (parser.Parse(client_buffer, readed_bytes, parsed)) {

        	                    // There is no command to be launched, continue to parse input stream
                	            // Here we are, current chunk finished some command, process it
		                       _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
	   			       _command_to_execute = parser.Build(arg_remains);
	   			       if (arg_remains > 0) {

	       				       arg_remains += 2;
	   			       }
				}

	                        // Parsed might fails to consume any bytes from input stream. In real life that could happens,
	                        // for example, because we are working with UTF-16 chars and only 1 byte left in stream
	                        if (parsed == 0) {

	                            break;
	                        } else {

	                            std::memmove(client_buffer, client_buffer + parsed, readed_bytes - parsed);
	                            readed_bytes -= parsed;
	                        }
			}

	                // There is command, but we still wait for argument to arrive...
			if (_command_to_execute && arg_remains > 0) {
		
				_logger->debug("Fill argument: {} bytes of {}", readed_bytes, arg_remains);
        	                // There is some parsed command, and now we are reading argument
                	        std::size_t to_read = std::min(arg_remains, std::size_t(readed_bytes));
                        	argument_for_command.append(client_buffer, to_read);

	                        std::memmove(client_buffer, client_buffer + to_read, readed_bytes - to_read);
	                        arg_remains -= to_read;
	                        readed_bytes -= to_read;
			}

                        // Thre is command & argument - RUN!
			if (_command_to_execute && arg_remains == 0) {				
	 			_logger->debug("Start command execution");

				std::string result;
        	                if (argument_for_command.size()){

                	            argument_for_command.resize(argument_for_command.size() - 2);
                        	}
				_command_to_execute->Execute(*pStorage, argument_for_command, result);

        	                // Send response
				result += "\r\n";

				size_t is_empty = q_commands.empty();
				q_commands.push_back(result);
				if( is_empty )
					_event.events |= EPOLLOUT;	

                	        // Prepare for the next command
                        	_command_to_execute.reset();
	                        argument_for_command.resize(0);
	                        parser.Reset();
			}
                } // while (readed_bytes)
     	}

    		if (readed_bytes == 0) {

			_logger->debug("Connection closed");
    		} else {

			throw std::runtime_error(std::string(strerror(errno)));
    		}

	std::atomic_thread_fence(std::memory_order_release); // Выгружаем кэш в оперативу.

        } catch (std::runtime_error &ex) {

    		_logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
        }
}

// See Connection.h
void Connection::DoWrite(){
	
	std::atomic_thread_fence(std::memory_order_acquire);

	try{
		if( q_commands.empty() ){
	
			_event.events |= EPOLLOUT;
		}
		else{
	
			std::string result = q_commands.front();
			q_commands.pop_front();
	                        
			// Send response
			if (send(_socket, result.data(), result.size(), 0) <= 0) {
				throw std::runtime_error("Failed to send response");
			}

		} 
	
		std::atomic_thread_fence(std::memory_order_release);

	}
	catch( std::runtime_error &ex ){

			_logger->error("Failed DoWrite function on current connection");
		}
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina
