#include "Webserv.hpp"
#include "ConfigParser.hpp"
#include <stdexcept>

int main(int argc, char** argv)
{
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [config-file]\n";
        return 1;
    }
    try {
        const std::string path = argc == 2 ? argv[1] : "config/default.conf";
        Config config = ConfigParser().parseFile(path);
        webserv::Server server(config);
        return server.run();
    } catch (const std::exception& error) {
        std::cerr << "Startup error: " << error.what() << '\n';
        return 1;
    }
}
