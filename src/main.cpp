// #include "ConfigParser.hpp"
// #include "EventLoop.hpp"
// #include "Logger.hpp"
// #include "Response.hpp"

// basic curl server
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

#include "HttpResponse.hpp"
#include <iostream>

void runGetTest(const std::string& label, const std::string& path) {
	Request req;
	req.method = Method::GET;
	req.path = path;

	Response res = Response::build(req, "./www");
	std::cout << "=== " << label << " (path: " << path << ") ===\n";
	std::cout << res.toString() << std::endl;
	std::cout << "-----------------------------\n" << std::endl;
}

void runPostTest(const std::string& label, const std::string& path, const std::string& body) {
	Request req;
	req.method = Method::POST;
	req.path = path;
	req.body = body;

	Response res = Response::build(req, "./www");
	std::cout << "=== " << label << " (path: " << path << ") ===\n";
	std::cout << res.toString() << std::endl;
	std::cout << "-----------------------------\n" << std::endl;
}

void runDeleteTest(const std::string& label, const std::string& path) {
	Request req;
	req.method = Method::DELETE;
	req.path = path;

	Response res = Response::build(req, "./www");
	std::cout << "=== " << label << " (path: " << path << ") ===\n";
	std::cout << res.toString() << std::endl;
	std::cout << "-----------------------------\n" << std::endl;
}

int main() {
	// runGetTest("existing file", "/index.html");
	// runGetTest("nonexistent file", "/doesnotexist.html");
	// runGetTest("directory (should serve index.html)", "/");
	// runGetTest("path traversal blocked", "/../main.cpp");
	// runGetTest("nested existing file", "/");

	runPostTest("create new file", "/upload_test.txt", "hello world");

	runDeleteTest("delete existing file", "/upload_test.txt");
}


// ssize_t recieve_message(int client_fd) {

// 	char buffer[4096];
// 	std::memset(buffer, 0, sizeof(buffer));
// 	ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) -1, 0);

// 	if (bytes_read == -1)
// 	{
// 		std::cerr << "recv() failed\n";
// 		return -1;
// 	}

// 	std::cout << "Received request:\n";
// 	std::cout << buffer << std::endl;
// 	return bytes_read;
// }

// void send_response(int client_fd, std::string response) {
// 	send(client_fd, response.c_str(), response.size(), 0);
// }

// int main()
// {
// 	//FOR REFERENCE:
// // struct pollfd {
// //     int fd;         // which fd to watch
// //     short events;   // what you care about
// //     short revents;  // what actually happened
// // };

// 	std::vector<pollfd> fds;
// 	int server_fd = socket(AF_INET, SOCK_STREAM, 0); // afinet = ipv4 domain. stream = TCP as opposote to something like udp

// 	fds.push_back({server_fd, POLLIN, 0});

// 	if (server_fd == -1)
// 	{
// 		std::cerr << "socket() failed\n";
// 		return (1);
// 	}

// 	std::cout << "Socket created: fd " << server_fd << std::endl;

// 	int opt = 1;
// 	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
// 	{
// 		std::cerr << "setsockopt() failed\n";
// 		close(server_fd);
// 		return 1;
// 	}

// 	sockaddr_in address = {}; // struct for ipv4 address
// 	address.sin_family = AF_INET;
// 	address.sin_addr.s_addr = INADDR_ANY; // accept connections on any network interface
// 	address.sin_port = htons(8080); // host to network short

// 	bind(server_fd, (sockaddr *)&address, sizeof(address));
// 	std::cout << "Bound to port " << ntohs(address.sin_port) << std::endl;

// 	if (listen(server_fd, 10) == -1)
// 	{
// 		std::cerr << "listen() failed\n";
// 		close(server_fd);
// 		return 1;
// 	}

// 	std::cout << "Listening on http://localhost:8080 ye yeeeee" << std::endl;

// 	while (true)
// 	{
// 		int ready = poll(fds.data(), fds.size(), -1); // params are arrayofpollfd, number of fds to watch, timeout (-1 = wait forever until sum happens)

// 		if (ready == -1)
// 		{
// 			perror("bad poll");
// 			return 1;
// 		}
// 		for (size_t i = 0; i < fds.size(); i++)
// 		{
// 			if (fds[i].revents & POLLIN) // events = what i asked to watch for, revents is what actually happened
// 			{
// 				if (fds[i].fd == server_fd)
// 				{
// 					std::cout << "New connection is pending" << std::endl;
// 					int client_fd = accept(server_fd, nullptr, nullptr);
// 					if (client_fd == -1)
// 					{
// 						std::cerr << "accept() failed\n";
// 						close(server_fd);
// 						return 1;
// 					}
// 					std::cout << "Client connected: fd " << client_fd << std::endl;
// 					fds.push_back({client_fd, POLLIN, 0});
// 				}
// 				else
// 				{
// 					// RECV REQUEST
// 					ssize_t bytes_read = recieve_message(fds[i].fd);
// 					if (bytes_read <= 0)
// 					{
// 						if (bytes_read == 0)
// 							std::cout << "Client disconnected" << std::endl;
// 						else
// 							perror("bad recv");
// 						close(fds[i].fd);
// 						fds.erase(fds.begin() + i);
// 						i--;
// 					}
// 					// SEND RESPONSE
// 					std::string response = build_response_HARDCODED(fds[i].fd);
// 					send_response(fds[i].fd, response);
// 					close(fds[i].fd);
// 					fds.erase(fds.begin() + i);
// 					i--;
// 				}
// 			}
// 		}
// 	}
// 	close(server_fd);
// 	return 0;
// }
