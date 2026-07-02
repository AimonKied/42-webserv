#pragma once

#include "Client.hpp"

#include <vector>
#include <poll.h>
#include <map>

int create_listening_socket(std::vector<pollfd>& fds);
int handle_new_connection(int server_fd, std::vector<pollfd>& fds, std::map<int, Client>& clients);