#include "Webserv.hpp"

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

	int server_fd = create_listening_socket(fds);

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
						if (bytes_read == 0)
							std::cout << "Client disconnected" << std::endl;
						else
							perror("bad recv");
						cleanup_client(fds, clients, i);
						continue;
					}
					if (request_complete(client.readBuffer))
					{
						// SEND RESPONSE
						client.state= ClientState::Writing;
						std::string response = build_response_HARDCODED(fds[i].fd);
						send_response(fds[i].fd, response);
						cleanup_client(fds, clients, i);
					}
					else
					{
						std::cout << "Incomplete request. Continuing..." << std::endl;
						for (char c : client.readBuffer)
						{
							if (c == '\r')
								std::cout << "\\r";
							else if (c == '\n')
								std::cout << "\\n";
							else
								std::cout << c;
						}
						std::cout << std::endl;
					}
				}
			}
		}
	}
	close(server_fd);
	return 0;
}
