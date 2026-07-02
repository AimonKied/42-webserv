#pragma once

#include <string>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <vector>
#include <map>

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

using webserv::Client;

std::string build_response_HARDCODED(int client_fd);
ssize_t recieve_message(Client& client);
bool request_complete(std::string buffer);
int handle_read_event(Client& client, pollfd& fd);
int handle_write_event(Client& client);
void cleanup_client(std::vector<pollfd>& fds, std::map<int, Client>& clients, size_t& i);
