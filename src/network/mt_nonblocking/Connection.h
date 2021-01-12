#ifndef AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H

#include <cstring>
#include <deque>
#include <memory>

#include <spdlog/logger.h>
#include <afina/Storage.h>

#include "protocol/Parser.h"
#include <afina/execute/Command.h>

#include <sys/epoll.h>

#include <memory>
#include <list>
#include <afina/Storage.h>
#include <spdlog/logger.h>
#include <afina/execute/Command.h>
#include <protocol/Parser.h>

namespace Afina {
namespace Network {
namespace STnonblock {

class Connection {
public:
    Connection( int s, 
		std::shared_ptr<spdlog::logger> &logger,
	        std::shared_ptr<Afina::Storage> &pStorage ) : _socket(s), 
							      _logger{logger},
                                                              pStorage{pStorage}{

        //std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
    }

    inline bool isAlive() const { return true; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;

    int _socket;
    struct epoll_event _event;
    std::deque<std::string> q_commands;
    
    std::shared_ptr<spdlog::logger> &_logger;
    std::shared_ptr<Afina::Storage> &pStorage;

    Protocol::Parser _parser;
    size_t _arg_remains;
    std::string _argument_for_command;
    std::unique_ptr<Execute::Command> _command_to_execute;    
    Protocol::Parser parser;
    size_t arg_remains;
    std::string argument_for_command;
    char client_buffer[4096];
    size_t offset_position;

};

} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
