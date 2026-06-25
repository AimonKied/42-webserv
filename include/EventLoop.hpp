#pragma once

#include "Client.hpp"
#include "Server.hpp"
#include <map>
#include <vector>

namespace webserv {

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    void run();
    void addServer(const Server &server);

private:
    std::vector<Server> servers;
    std::map<int, Client> clients;
};

} // namespace webserv
