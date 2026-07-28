#include "Server.hpp"
#include "HttpParser.hpp"
#include "ConfigTypes.hpp"

#include <cerrno>

namespace webserv {

volatile std::sig_atomic_t Server::_signalReceived = 0;

Server::Server(int port)
	: _running(false), _port(port), _serverFd(-1), _fds(), _clients()
{
}

Server::~Server()
{
	closeAllFds();
}

int Server::run()
{
	_running = true;
	_signalReceived = 0;
	if (setupSignalHandlers() < 0)
		return 1;
	if (createListeningSocket() < 0)
	{
		closeAllFds();
		return 1;
	}

	while (_running)
	{
		if (_signalReceived)
		{
			std::cout << "\nShutdown signal received, stopping server..." << std::endl;
			_running = false;
			break;
		}

		int ready = poll(_fds.data(), _fds.size(), 1000);
		if (ready == -1)
		{
			if (errno == EINTR)
				continue;
			perror("bad poll");
			closeAllFds();
			return 1;
		}

		for (size_t i = 0; i < _fds.size(); ++i)
		{
			checkClientTimeouts();
			int fd = _fds[i].fd;
			short revents = _fds[i].revents;

			if (revents == 0)
				continue;
			if (revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				if (fd == _serverFd)
				{
					std::cerr << "Fatal error on listening socket\n";
					closeAllFds();
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
					{
						closeAllFds();
						return 1;
					}
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
	closeAllFds();
	return 0;
}

void Server::handleSignal(int signal)
{
	(void)signal;
	_signalReceived = 1;
}

int Server::setupSignalHandlers()
{
	if (std::signal(SIGINT, Server::handleSignal) == SIG_ERR)
	{
		std::cerr << "failed to install SIGINT handler\n";
		return -1;
	}
	if (std::signal(SIGTERM, Server::handleSignal) == SIG_ERR)
	{
		std::cerr << "failed to install SIGTERM handler\n";
		return -1;
	}
	return 0;
}

int Server::createListeningSocket()
{
	_serverFd = socket(AF_INET, SOCK_STREAM, 0);
	setNonBlocking(_serverFd);
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
	setNonBlocking(clientFd);
	if (clientFd == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0; // fake news from poll client not ready
		std::cerr << "accept() failed\n";
		return -1;
	}
	std::cout << "Client connected: fd " << clientFd << std::endl;

	_fds.push_back({clientFd, POLLIN, 0});
	_clients.emplace(clientFd, Client(clientFd));
	return 0;
}

/* Hardcoded Configfile-Filler, bis Configfile-Parser fertig ist */
Config hardcodedConfig()
{
	LocationConfig root;
	root.path = "/";
	root.root = "./www";
	root.index = "index.html";
	root.methods.push_back(Method::GET);
	root.methods.push_back(Method::POST);
	root.methods.push_back(Method::DELETE);

	ServerConfig server;
	server.host = "0.0.0.0";
	server.port = 8080;
	server.serverName = "localhost";
	server.clientMaxBodySize = 1024 * 1024;
	server.locations.push_back(root);

	Config config;
	config.push_back(server);
	return config;
}

int Server::handleClientRead(size_t& i)
{
	Client& client = _clients.at(_fds[i].fd);
	ssize_t bytesRead = client.receive();

	if (bytesRead <= 0)
	{
		if (bytesRead == 0)
		{
			std::cout << "Client at fd " << _fds[i].fd << " disconnected" << std::endl;
			cleanupClient(i);
			return -1;
		}
		if (bytesRead == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return 0; // fake news from poll client not ready
			else
				perror("bad recv");
			cleanupClient(i);
		}
		return -1;
	}

	HttpParser parser;
	Request request = parser.parse(client.readBuffer, client.MAX_REQUEST_SIZE);

	if (!request.complete)
	{
		std::cout << "Incomplete request. Continuing..." << std::endl;
		return 0;
	}

	client.state = ClientState::Writing;
	client.writeBuffer = buildResponse(client.fd);
	client.readBuffer.clear();
	_fds[i].events = POLLOUT;
	return 0;
}

int Server::handleClientWrite(size_t& i)
{
	Client& client = _clients.at(_fds[i].fd);
	ssize_t bytesSent = client.sendChunk();

	if (bytesSent < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;
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

void Server::closeAllFds()
{
	for (size_t i = 0; i < _fds.size(); ++i)
	{
		if (_fds[i].fd >= 0)
			close(_fds[i].fd);
	}
	_fds.clear();
	_clients.clear();
	_serverFd = -1;
}

std::string Server::buildResponse(int clientFd) const // remove this and replace with Simons
{
	(void)clientFd;

	std::string body = "Hello there budster!\n";
	return "HTTP/1.1 200 OK\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " + std::to_string(body.size()) + "\r\n"
		"\r\n" +
		body;
}

int Server::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return -1;
	
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		return -1;
	return 0;
}

void Server::checkClientTimeouts()
{
	std::time_t now = std::time(nullptr);

	for (size_t i = 0; i < _fds.size(); ++i)
	{
		int fd = _fds[i].fd;

		if (fd == _serverFd)
			continue;
		
		Client& client = _clients.at(fd);

		if (now - client.lastActivity > CLIENT_TIMEOUT_SECONDS)
		{
			std::cout << "Client at fd: " << fd << " timed out" << std::endl;
			cleanupClient(i);
			--i;
		}
	}

}

} // namespace webserv
