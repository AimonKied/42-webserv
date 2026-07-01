#include "Client.hpp"

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
	return buffer.find("\r\n\r\n") != std::string::npos;
}