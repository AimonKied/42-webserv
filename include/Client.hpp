#pragma once

#include <string>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace webserv {

enum class ClientState {
    Reading,
    Writing,
    Closed
};

class Client {
public:
    explicit Client(int fd);

    ssize_t receive();
    ssize_t sendChunk();

    int fd;
    std::string readBuffer;
    std::string writeBuffer;
    ClientState state;
};

} // namespace webserv
