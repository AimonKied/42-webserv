#include "ConfigParser.hpp"
#include "EventLoop.hpp"
#include "Logger.hpp"

// basic curl server
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

ssize_t recieve_message(int client_fd) {

	char buffer[4096];
	std::memset(buffer, 0, sizeof(buffer));
	ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) -1, 0);

	if (bytes_read == -1)
	{
		std::cerr << "recv() failed\n";
		return -1;
	}

	std::cout << "Received request:\n";
	std::cout << buffer << std::endl;
	return bytes_read;
}

void build_and_send(int client_fd) {
	
	std::string body = "Hello there budster!\n";
	std::string response =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " + std::to_string(body.size()) + "\r\n"
		"\r\n" +
		body;

	send(client_fd, response.c_str(), response.size(), 0);
}


int main()
{
	//FOR REFERENCE:
// struct pollfd {
//     int fd;         // which fd to watch
//     short events;   // what you care about
//     short revents;  // what actually happened
// };

	std::vector<pollfd> fds;
	int server_fd = socket(AF_INET, SOCK_STREAM, 0); // afinet = ipv4 domain. stream = TCP as opposote to something like udp

	fds.push_back({server_fd, POLLIN, 0});

	if (server_fd == -1)
	{
		std::cerr << "socket() failed\n";
		return (1);
	}

	std::cout << "Socket created: fd " << server_fd << std::endl;

	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
	{
		std::cerr << "setsockopt() failed\n";
		close(server_fd);
		return 1;
	}

	sockaddr_in address = {}; // struct for ipv4 address
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY; // accept connections on any network interface
	address.sin_port = htons(8080); // host to network short

	bind(server_fd, (sockaddr *)&address, sizeof(address));
	std::cout << "Bound to port " << ntohs(address.sin_port) << std::endl;

	if (listen(server_fd, 10) == -1)
	{
		std::cerr << "listen() failed\n";
		close(server_fd);
		return 1;
	}

	std::cout << "Listening on http://localhost:8080 ye yeeeee" << std::endl;

	while (true)
	{
		int ready = poll(fds.data(), fds.size(), -1); // params are arrayofpollfd, number of fds to watch, timeout (-1 = wait forever until sum happens)

		if (ready == -1)
		{
			perror("bad poll");
			return 1;
		}
		for (size_t i = 0; i < fds.size(); i++)
		{
			if (fds[i].revents & POLLIN) // events = what i asked to watch for, revents is what actually happened
			{
				if (fds[i].fd == server_fd)
				{
					std::cout << "New connection is pending" << std::endl;
					int client_fd = accept(server_fd, nullptr, nullptr);
					if (client_fd == -1)
					{
						std::cerr << "accept() failed\n";
						close(server_fd);
						return 1;
					}
					std::cout << "Client connected: fd " << client_fd << std::endl;
					fds.push_back({client_fd, POLLIN, 0});
				}
				else
				{
					// RECV REQUEST
					ssize_t bytes_read = recieve_message(fds[i].fd);
					if (bytes_read <= 0)
					{
						if (bytes_read == 0)
							std::cout << "Client disconnected" << std::endl;
						else
							perror("bad recv");
						close(fds[i].fd);
						fds.erase(fds.begin() + i);
						i--;
					}
					// SEND RESPONSE
					build_and_send(fds[i].fd);

					close(fds[i].fd);
					fds.erase(fds.begin() + i);
					i--;
				}
			}
		}
	}
	close(server_fd);
	return 0;
}
