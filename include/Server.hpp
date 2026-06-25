#pragma once

#include "Types.hpp"
#include "Logger.hpp"
#include <string>
#include <vector>

namespace webserv {

class Server {
public:
    Server();
    ~Server();

    std::string host;
    unsigned short port;
    std::string root;
    std::vector<std::string> routes;
    int listenSocket;
};

} // namespace webserv
