#include "Webserv.hpp"
#include <vector>

int main()
{
	std::vector<int> ports = {8080, 8081};
	webserv::Server server(ports);

	return server.run();
}
