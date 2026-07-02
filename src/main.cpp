#include "Webserv.hpp"

int main()
{
	webserv::Server server(8080);

	return server.run();
}
