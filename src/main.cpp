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
        // Multiple listeners and location selection are the next integration steps.
        if (config.size() != 1)
            throw std::runtime_error("This runtime currently supports one server block");
        if (config[0].locations.size() != 1 || config[0].locations[0].path != "/")
            throw std::runtime_error("This runtime currently requires one location at '/'");
        webserv::Server server(config[0]);
        return server.run();
    } catch (const std::exception& error) {
        std::cerr << "Startup error: " << error.what() << '\n';
        return 1;
    }
}
