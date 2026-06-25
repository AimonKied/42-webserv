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
    Client(int socketFd);
    ~Client();

    int socketFd;
    std::string readBuffer;
    std::string writeBuffer;
    ClientState state;
    Request request;
    Response response;
};

} // namespace webserv
