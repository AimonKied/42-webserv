#include "Webserv.hpp"

// basic curl server

using webserv::Client;
using webserv::ClientState;

int main()
{
	//FOR REFERENCE:
// struct pollfd {
//     int fd;         // which fd to watch
//     short events;   // what you care about
//     short revents;  // what actually happened
// };

	std::vector<pollfd> fds;
	std::map<int, Client> clients;

	int server_fd = create_listening_socket(fds);
	if (server_fd < 0)
	{
		perror("bad listening socket");
		return 1;
	}

	while (true)
	{
		int ready = poll(fds.data(), fds.size(), -1); // params are arrayofpollfd, number of fds to watch, timeout (-1 = wait forever until sum happens)
		if (ready == -1)
		{
			perror("bad poll");
			return 1;
		}

		for (size_t i = 0; i < fds.size(); i++)
		{
			int fd = fds[i].fd;
			short revents = fds[i].revents;

			if (revents & POLLIN) // events = what i asked to watch for, revents is what actually happened
			{
				if (fd == server_fd)
				{
					if (handle_new_connection(server_fd, fds, clients) < 0)
						return(perror("bad connection handle"), 1);
				}
				else
				{
					Client& client = clients.at(fd);
					if (client.state == ClientState::Reading
						&& handle_read_event(client, fds[i]) < 0)
					{
						cleanup_client(fds, clients, i);
						continue;
					}
				}
			}
			if (revents & POLLOUT)
			{
				if (fd == server_fd)
					continue;
				Client& client = clients.at(fd);
				if (client.state == ClientState::Writing)
				{
					int result = handle_write_event(client);
					if (result < 0 || result > 0)
					{
						cleanup_client(fds, clients, i);
						continue;
					}
				}
			}
		}
	}
	close(server_fd);
	return 0;
}
