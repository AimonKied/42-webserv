#include "Client.hpp"

namespace webserv {

Client::Client(int fd)
	: fd(fd), readBuffer(""), writeBuffer(""), state(ClientState::Reading)
{
}

ssize_t Client::receive()
{
	char buffer[4096];

	std::memset(buffer, 0, sizeof(buffer));
	ssize_t bytesRead = recv(fd, buffer, sizeof(buffer), 0);
	if (bytesRead > 0)
		readBuffer.append(buffer, bytesRead);
	return bytesRead;
}

ssize_t Client::sendChunk()
{
	ssize_t bytesSent = send(fd, writeBuffer.c_str(), writeBuffer.size(), 0);

	if (bytesSent > 0)
		writeBuffer.erase(0, bytesSent);
	return bytesSent;
}

} // namespace webserv
