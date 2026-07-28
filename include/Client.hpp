#pragma once

#include <string>

namespace webserv {

enum class ClientState {
    Reading,
    Writing,
    Closed
};

class Client {
public:
    explicit Client(int fd);

    int fd;
    std::string readBuffer;
    std::string writeBuffer;
    ClientState state;
};

}


std::string build_response_HARDCODED(int client_fd);