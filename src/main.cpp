#include "ConfigParser.hpp"
#include "EventLoop.hpp"
#include "Logger.hpp"

int main() {
    webserv::Logger logger;
    logger.info("Starting webserv skeleton");

    webserv::ConfigParser parser;
    webserv::EventLoop loop;

    std::vector<webserv::Server> servers = parser.parseFile("config/default.conf");
    for (std::vector<webserv::Server>::const_iterator it = servers.begin(); it != servers.end(); ++it) {
        loop.addServer(*it);
    }

    loop.run();
    return 0;
}
