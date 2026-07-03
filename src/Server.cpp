#include "Server.hpp"

namespace webserv {

Server::Server(int port)
	: _port(port), _serverFd(-1), _fds(), _clients()
{
}

Server::~Server()
{
	for (size_t i = 0; i < _fds.size(); ++i)
		close(_fds[i].fd);
}

int Server::run()
{
	if (createListeningSocket() < 0)
		return 1;

	while (true)
	{
		int ready = poll(_fds.data(), _fds.size(), -1);
		if (ready == -1)
		{
			perror("bad poll");
			return 1;
		}

		for (size_t i = 0; i < _fds.size(); ++i)
		{
			int fd = _fds[i].fd;
			short revents = _fds[i].revents;

			if (revents == 0)
				continue;
			if (revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				if (fd == _serverFd)
				{
					std::cerr << "Fatal error on listening socket\n";
					return 1;
				}

				if (revents & POLLHUP)
					std::cerr << "Client " << fd << " disconnected\n";
				else
					std::cerr << "Client " << fd << " experienced an error\n";
				cleanupClient(i);
				continue;
			}
			if (revents & POLLIN)
			{
				if (fd == _serverFd)
				{
					if (acceptClient() < 0)
						return 1;
				}
				else if (_clients.at(fd).state == ClientState::Reading && handleClientRead(i) < 0)
					continue;
			}
			if (revents & POLLOUT)
			{
				if (fd != _serverFd && _clients.at(fd).state == ClientState::Writing && handleClientWrite(i) != 0)
					continue;
			}

		}
	}
	return 0;
}

int Server::createListeningSocket()
{
	_serverFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_serverFd == -1)
	{
		std::cerr << "socket() failed\n";
		return -1;
	}
	std::cout << "Socket created: fd " << _serverFd << std::endl;

	int opt = 1;
	if (setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
	{
		std::cerr << "setsockopt() failed\n";
		close(_serverFd);
		_serverFd = -1;
		return -1;
	}

	sockaddr_in address = {};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(_port);

	if (bind(_serverFd, (sockaddr *)&address, sizeof(address)) == -1)
	{
		perror("bind() failed");
		close(_serverFd);
		_serverFd = -1;
		return -1;
	}
	std::cout << "Bound to port " << ntohs(address.sin_port) << std::endl;

	if (listen(_serverFd, 10) == -1)
	{
		std::cerr << "listen() failed\n";
		close(_serverFd);
		_serverFd = -1;
		return -1;
	}

	_fds.push_back({_serverFd, POLLIN, 0});
	std::cout << "Listening on http://localhost:" << _port << " ye yeeeee" << std::endl;
	return 0;
}

int Server::acceptClient()
{
	std::cout << "New connection is pending" << std::endl;

	int clientFd = accept(_serverFd, NULL, NULL);
	if (clientFd == -1)
	{
		std::cerr << "accept() failed\n";
		return -1;
	}
	std::cout << "Client connected: fd " << clientFd << std::endl;

	_fds.push_back({clientFd, POLLIN, 0});
	_clients.emplace(clientFd, Client(clientFd));
	return 0;
}

int Server::handleClientRead(size_t& i)
{
	Client& client = _clients.at(_fds[i].fd);
	ssize_t bytesRead = client.receive();

	if (bytesRead <= 0)
	{
		if (bytesRead == 0)
			std::cout << "Client at fd " << _fds[i].fd << " disconnected" << std::endl;
		else
			perror("bad recv");
		cleanupClient(i);
		return -1;
	}
	if (requestComplete(client.readBuffer))
	{
		client.state = ClientState::Writing;
		client.writeBuffer = buildResponse(client.fd);
		client.readBuffer.clear();
		_fds[i].events = POLLOUT;
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

int Server::handleClientWrite(size_t& i)
{
	Client& client = _clients.at(_fds[i].fd);
	ssize_t bytesSent = client.sendChunk();

	if (bytesSent < 0)
	{
		perror("bad byte send");
		cleanupClient(i);
		return -1;
	}
	if (client.writeBuffer.empty())
	{
		cleanupClient(i);
		return 1;
	}
	return 0;
}

void Server::cleanupClient(size_t& i)
{
	close(_fds[i].fd);
	_clients.erase(_fds[i].fd);
	_fds.erase(_fds.begin() + i);
	if (i > 0)
		--i;
}

bool Server::requestComplete(const std::string& buffer) const
{
	return buffer.find("\r\n\r\n") != std::string::npos
		|| buffer.find("\n\n") != std::string::npos;
}

std::string Server::buildResponse(int clientFd) const
{
	(void)clientFd;

	std::string body = "Hello there budster!\n";
	return "HTTP/1.1 200 OK\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " + std::to_string(body.size()) + "\r\n"
		"\r\n" +
		body;
}

} // namespace webserv
