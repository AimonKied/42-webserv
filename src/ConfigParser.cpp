#include "ConfigParser.hpp"

namespace webserv {

ConfigParser::ConfigParser() {
}

ConfigParser::~ConfigParser() {
}

std::vector<Server> ConfigParser::parseFile(const std::string &path) const {
    (void)path;
    // TODO: parse nginx-like configuration file and build Server objects
    return std::vector<Server>();
}

} // namespace webserv
