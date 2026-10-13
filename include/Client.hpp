#pragma once

#include <string>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <ctime>
#include <iostream>

struct LocationConfig;

namespace webserv {

enum class ClientState {
	Reading,
	Writing,
	WaitingForCgi,
	Closed
};

class Client {
public:
	explicit Client(int fd, int listenerFd);

	ssize_t receive();
	ssize_t sendChunk();

	int fd;
	int listenerFd;
	// Retained while CGI runs; points into Server's immutable configuration.
	const LocationConfig* cgiLocation = nullptr;
	std::string readBuffer;
	std::string writeBuffer;
	ClientState state;
	std::time_t lastActivity;
};

} // namespace webserv
