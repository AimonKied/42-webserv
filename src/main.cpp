#include "Client.hpp"

// basic curl server

using webserv::Client;
using webserv::ClientState;


void send_response(int client_fd, std::string response) {
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
	std::map<int, Client> clients;


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
					clients.emplace(client_fd, Client(client_fd));
				}
				else
				{
					// RECV REQUEST
					int fd = fds[i].fd;
					Client& client = clients.at(fd);
					ssize_t bytes_read = recieve_message(client);
					if (bytes_read <= 0)
					{
						// CLEANUP CLIENT
						if (bytes_read == 0)
							std::cout << "Client disconnected" << std::endl;
						else
							perror("bad recv");
						close(fds[i].fd);
						clients.erase(fds[i].fd);
						fds.erase(fds.begin() + i);
						i--;
						continue;
					}
					if (request_complete(client.readBuffer))
					{
						// SEND RESPONSE
						client.state= ClientState::Writing;
						std::string response = build_response_HARDCODED(fds[i].fd);
						send_response(fds[i].fd, response);
						// CLEANUP CLIENT turn into function as above
						close(fds[i].fd);
						fds.erase(fds.begin() + i);
						i--;
					}
				}
			}
		}
	}
	close(server_fd);
	return 0;
}
