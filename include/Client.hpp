#pragma once

#include "Request.hpp"
#include "Response.hpp"
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

} // namespace webserv
