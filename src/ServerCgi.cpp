#include "Server.hpp"
#include "Cgi.hpp"

namespace webserv {

void Server::queueResponse(Client& client, const Response& response) {
	client.writeBuffer = response.toString();
	client.readBuffer.clear();
	client.state = ClientState::Writing;
	client.cgiLocation = nullptr;
	client.lastActivity = std::time(nullptr);
	for (pollfd& entry : _fds)
		if (entry.fd == client.fd) entry.events = POLLOUT;
}

void Server::collectCgiResults() {
	for (CgiResult& result : _cgi.collectFinished()) {
		auto client = _clients.find(result.clientFd);
		if (client == _clients.end() || client->second.state != ClientState::WaitingForCgi)
			continue;
		const LocationConfig& location = *client->second.cgiLocation;
		queueResponse(client->second, result.errorCode
			? makeErrorResponse(result.errorCode, location)
			: buildCgiResponse(result.output, location));
	}
}
} // namespace webserv
