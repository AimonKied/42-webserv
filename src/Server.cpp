#include "Server.hpp"
#include "HttpParser.hpp"
#include "ConfigTypes.hpp"

#include <cerrno>

namespace webserv {

volatile std::sig_atomic_t Server::_signalReceived = 0;

Server::Server(const Config& config)
	: _running(false), _config(config), _fds(), _clients()
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
	for (std::size_t configIndex = 0; configIndex < _config.size(); ++configIndex)
	{
		if (createListeningSocket(configIndex) < 0)
		{
			closeAllFds();
			return 1;
		}
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

		checkClientTimeouts();
		for (size_t i = 0; i < _fds.size(); ++i)
		{
			int fd = _fds[i].fd;
			short revents = _fds[i].revents;

			if (revents == 0)
				continue;
			if (revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				if (isListener(fd))
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
				if (isListener(fd))
				{
					if (acceptClient(fd) < 0)
					{
						perror("acceptClient() failed");
						return 1;
					}
				}
				else if (_clients.at(fd).state == ClientState::Reading && handleClientRead(i) < 0)
					continue;
			}
			if (revents & POLLOUT)
			{
				if (!isListener(fd) && _clients.at(fd).state == ClientState::Writing && handleClientWrite(i) != 0)
					continue;
			}

		}
	}
	closeAllFds();
	return 0;
}

bool Server::isListener(int fd) const
{
	return _listeners.find(fd) != _listeners.end();
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
	if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		std::cerr << "failed to install SIGPIPE handler\n";
		return -1;
	}
	return 0;
}

int Server::createListeningSocket(std::size_t configIndex)
{
	const int port = _config[configIndex].port;
	int listenerFd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenerFd == -1)
	{
		std::cerr << "socket() failed\n";
		return -1;
	}
	std::cout << "Socket created: fd " << listenerFd << std::endl;
	if (setNonBlocking(listenerFd) == -1)
	{
		std::cerr << "setNonBlocking() failed\n";
		close(listenerFd);
		return -1;
	}
	int opt = 1;
	if (setsockopt(listenerFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
	{
		std::cerr << "setsockopt() failed\n";
		close(listenerFd);
		return -1;
	}

	sockaddr_in address = {};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(listenerFd, (sockaddr *)&address, sizeof(address)) == -1)
	{
		perror("bind() failed");
		close(listenerFd);
		return -1;
	}
	std::cout << "Bound to port " << ntohs(address.sin_port) << std::endl;

	if (listen(listenerFd, 10) == -1)
	{
		std::cerr << "listen() failed\n";
		close(listenerFd);
		return -1;
	}

	_fds.push_back({listenerFd, POLLIN, 0});
	_listeners[listenerFd] = configIndex;
	std::cout << "Listening on http://localhost:" << port << " ye yeeeee" << std::endl;
	return 0;
}

int Server::acceptClient(int listenerFd)
{
	std::cout << "New connection is pending" << std::endl;

	int clientFd = accept(listenerFd, NULL, NULL);
	if (clientFd == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 1; // fake news from poll client not ready
		std::cerr << "accept() failed\n";
		return 1;
	}
	std::cout << "Client connected: fd " << clientFd << std::endl;
	if (setNonBlocking(clientFd) == -1)
	{
		std::cerr << "setNonBlocking() failed\n";
		close(clientFd);
		return 1;
	}
	_fds.push_back({clientFd, POLLIN, 0});
	_clients.emplace(clientFd, Client(clientFd, listenerFd));
	return 0;
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
	const ServerConfig& serverConfig = _config.at(_listeners.at(client.listenerFd));
	const std::size_t maxRequestOverhead = 64 * 1024;
	if (client.readBuffer.size() > serverConfig.clientMaxBodySize
		&& client.readBuffer.size() - serverConfig.clientMaxBodySize > maxRequestOverhead)
	{
		std::cerr << "Request buffer limit exceeded\n";
		cleanupClient(i);
		return -1;
	}
	HttpParser parser;
	Request request = parser.parse(client.readBuffer, serverConfig.clientMaxBodySize);

	if (!request.complete && request.errorCode == 0)
	{
		std::cout << "Incomplete request. Continuing..." << std::endl;
		return 0;
	}

	client.state = ClientState::Writing;
	client.writeBuffer = buildResponse(request, serverConfig.locations[0]);
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
	_listeners.clear();
}

std::string Server::buildResponse(const Request& request, const LocationConfig& location) const
{
	return Response::build(request, location).toString();
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

		if (isListener(fd))
			continue;
		
		Client& client = _clients.at(fd);

		if (now - client.lastActivity > CLIENT_TIMEOUT_SECONDS)
		{
			std::cout << "Client at fd: " << fd << " timed out" << std::endl;
			cleanupClient(i);
		}
	}

}

} // namespace webserv
