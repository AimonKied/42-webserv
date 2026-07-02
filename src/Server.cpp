#include "Webserv.hpp"

int create_listening_socket(std::vector<pollfd>& fds)
{
	int server_fd = socket(AF_INET, SOCK_STREAM, 0); // afinet = ipv4 domain. stream = TCP as opposote to something like udp

	fds.push_back({server_fd, POLLIN, 0});

	if (server_fd == -1)
	{
		std::cerr << "socket() failed\n";
		return (-1);
	}

	std::cout << "Socket created: fd " << server_fd << std::endl;

	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
	{
		std::cerr << "setsockopt() failed\n";
		close(server_fd);
		return -1;
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
		return -1;
	}

	std::cout << "Listening on http://localhost:8080 ye yeeeee" << std::endl;
	return server_fd;
}

int handle_new_connection(int server_fd, std::vector<pollfd>& fds, std::map<int, Client>& clients)
{

	std::cout << "New connection is pending" << std::endl;
	int client_fd = accept(server_fd, nullptr, nullptr);
	if (client_fd == -1)
	{
		std::cerr << "accept() failed\n";
		close(server_fd);
		return -1;
	}
	std::cout << "Client connected: fd " << client_fd << std::endl;
	fds.push_back({client_fd, POLLIN, 0});
	clients.emplace(client_fd, Client(client_fd));
	return 0;
}
