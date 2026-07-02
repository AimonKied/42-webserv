#pragma once

#include <vector>
#include <poll.h>

int create_listening_socket(std::vector<pollfd>& fds);