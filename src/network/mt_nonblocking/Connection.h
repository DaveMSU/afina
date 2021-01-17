#ifndef AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H

#include <cstring>

#include <sys/epoll.h>

#include <memory>
#include <list>
#include <afina/Storage.h>
#include <spdlog/logger.h>
#include <afina/execute/Command.h>
#include <protocol/Parser.h>

namespace Afina {
namespace Network {
namespace MTnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage> pstore, std::shared_ptr<spdlog::logger> l) : _socket(s), pStorage(pstore), _logger(l) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
        readed_bytes = 0;
        offseti = 0;
        offseto = 0;
        arg_remains = 0;
        alive = false;
    }

    inline bool isAlive() const { return alive; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;
    friend class Worker;

    int _socket;
    struct epoll_event _event;
    bool alive;

    static const std::size_t bufflen = 2048;
    char rbuffer[bufflen];
    int readed_bytes;
    std::size_t offseti;
    std::size_t offseto;

    std::shared_ptr<Afina::Storage> pStorage;
    std::shared_ptr<spdlog::logger> _logger;
    std::list<std::string> answers;
    std::size_t arg_remains;
    Protocol::Parser parser;
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;
};

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
