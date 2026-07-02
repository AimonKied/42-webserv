#include "Webserv.hpp"

namespace webserv{


Client::Client(int fd)
	: fd(fd), readBuffer(""), writeBuffer(""), state(ClientState::Reading)
{
}

}

using webserv::Client;

std::string build_response_HARDCODED(int client_fd) {
	
	(void)client_fd;
	std::string body = "Hello there budster!\n";
	std::string response =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " + std::to_string(body.size()) + "\r\n"
		"\r\n" +
		body;
	return (response);
}

ssize_t recieve_message(Client& client)
{
	char buffer[4096];
	std::memset(buffer, 0, sizeof(buffer));
	ssize_t bytes_read = recv(client.fd, buffer, sizeof(buffer), 0);

	if (bytes_read > 0)
		client.readBuffer.append(buffer, bytes_read);
	
	return bytes_read;
}

bool request_complete(std::string buffer)
{
	return buffer.find("\r\n\r\n") != std::string::npos || buffer.find("\n\n") != std::string::npos;
}

int handle_read_event(Client& client, pollfd& fd)
{
	ssize_t bytes_read = recieve_message(client);
	if (bytes_read <= 0)
	{
		if (bytes_read == 0)
			std::cout << "Client disconnected" << std::endl;
		else
			perror("bad recv");
		return -1;
	}
	if (request_complete(client.readBuffer))
	{
		client.state = webserv::ClientState::Writing;
		client.writeBuffer = build_response_HARDCODED(client.fd);
		client.readBuffer.clear();
		fd.events = POLLOUT;
		return 0;
	}
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
	return 0;
}

int handle_write_event(Client& client)
{
	ssize_t bytes_sent = send(client.fd, client.writeBuffer.c_str(), client.writeBuffer.size(), 0);
	if (bytes_sent < 0)
		return (perror("bad byte send"), -1);
	client.writeBuffer.erase(0, bytes_sent);
	if (client.writeBuffer.empty())
		return 1;
	return 0;
}

void cleanup_client(std::vector<pollfd>& fds, std::map<int, Client>& clients, size_t& i)
{

	close(fds[i].fd);
	clients.erase(fds[i].fd);
	fds.erase(fds.begin() + i);
	if (i > 0)
		--i;
}
