#include "Connection.h"

#include <iostream>
#include <unistd.h>
#include <sys/uio.h>
#include <atomic>

namespace Afina {
namespace Network {
namespace MTnonblock {

// See Connection.h
void Connection::Start() {
    //std::cout << "Start" << std::endl;
    _event.events = EPOLLIN;
    alive = true;
}

// See Connection.h
void Connection::OnError() {
    //std::cout << "OnError" << std::endl;
    alive = false;
}

// See Connection.h
void Connection::OnClose() {
    //std::cout << "OnClose" << std::endl;
    alive = false;
}

// See Connection.h
void Connection::DoRead() {
    //std::cout << "DoRead" << std::endl;
    std::atomic_thread_fence(std::memory_order_acquire);
    try {
        //check eagain
        while ((readed_bytes = read(_socket, &rbuffer[offseti], bufflen - offseti)) > 0) {
            _logger->debug("Got {} bytes from socket", readed_bytes);
            readed_bytes += offseti;
            offseti = 0;

            // Single block of data readed from the socket could trigger inside actions a multiple times,
            // for example:
            // - read#0: [<command1 start>]
            // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
            while (readed_bytes > 0) {
                _logger->debug("Process {} bytes", readed_bytes);
                // There is no command yet
                if (!command_to_execute) {
                    std::size_t parsed = 0;
                    if (parser.Parse(rbuffer, readed_bytes, parsed)) {
                        // There is no command to be launched, continue to parse input stream
                        // Here we are, current chunk finished some command, process it
                        _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                        command_to_execute = parser.Build(arg_remains);
                        if (arg_remains > 0) {
                            arg_remains += 2;
                        }
                    }

                    // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                    // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                    if (parsed == 0) {
                        offseti = readed_bytes;
                        break;
                    } 
                    else {
                        std::memmove(rbuffer, rbuffer + parsed, readed_bytes - parsed);
                        readed_bytes -= parsed;
                    }
                }

                // There is command, but we still wait for argument to arrive...
                if (command_to_execute && arg_remains > 0) {
                    _logger->debug("Fill argument: {} bytes of {}", readed_bytes, arg_remains);
                    // There is some parsed command, and now we are reading argument
                    std::size_t to_read = std::min(arg_remains, std::size_t(readed_bytes));
                    argument_for_command.append(rbuffer, to_read);

                    std::memmove(rbuffer, rbuffer + to_read, readed_bytes - to_read);
                    arg_remains -= to_read;
                    readed_bytes -= to_read;
                }

                // Thre is command & argument - RUN!
                if (command_to_execute && arg_remains == 0) {
                    
                    _logger->debug("Start command execution");

                    std::string result;
                    command_to_execute->Execute(*pStorage, argument_for_command, result);

                    // Save response
                    result += "\r\n";
                    answers.push_back(result);

                    // Prepare for the next command
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            } // while (readed_bytes > 0)
        } // while(read > 0)
        if (answers.size() != 0) {
            _event.events |= EPOLLOUT;
        }
        if (readed_bytes == 0) {
            _logger->debug("Connection closed");
        }
        else if (errno != EWOULDBLOCK && errno != EAGAIN) {
            throw std::runtime_error(std::string(strerror(errno)));
        }
        std::atomic_thread_fence(std::memory_order_release);
    }
    catch (std::runtime_error &ex) {
        std::string message("SERVER ERROR ");
        message += ex.what();
        message += "\r\n";
        _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
        write(_socket, message.data(), message.size());
        OnError();
    }
}

// See Connection.h
void Connection::DoWrite() {
    //std::cout << "DoWrite" << std::endl;
    std::atomic_thread_fence(std::memory_order_acquire);
    size_t count = answers.size();
    iovec tmp [count];
    int head_written_count = 0;
    {
        std::size_t i = 0;
        for (auto it = answers.begin(); it != answers.end(); it++) {
            tmp[i].iov_base = &((*it)[0]);
            tmp[i].iov_len = it->size();
            i++;
        }
    }

    tmp[0].iov_base = (char*)tmp[0].iov_base + offseto;
    tmp[0].iov_len -= offseto;

    head_written_count = writev(_socket, tmp, count);

    if (head_written_count == -1 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)){
        return;
    }
    if (head_written_count < 0) {
        std::string err;
        err = strerror(errno);
        _logger->error("Failed to send response on descriptor {}: {}", _socket, err);
        OnError();
        return;
    }

    while (head_written_count > 0) {
        if (head_written_count >= answers.front().size()) {
            head_written_count -= answers.front().size();
            answers.pop_front();
        }
        else
            break;
    }
    offseto = head_written_count;
    if (answers.size() == 0) {
        _event.events = EPOLLIN & ~EPOLLOUT;
        alive = false;
    }
    std::atomic_thread_fence(std::memory_order_release);
}

} // namespace MTnonblock
} // namespace Network
} // namespace Afina
