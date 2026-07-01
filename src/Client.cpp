#include "Client.hpp"

namespace webserv {

} // namespace webserv

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
