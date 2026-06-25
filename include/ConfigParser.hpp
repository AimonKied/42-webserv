#pragma once

#include "Server.hpp"
#include <string>
#include <vector>

namespace webserv {

class ConfigParser {
public:
    ConfigParser();
    ~ConfigParser();

    std::vector<Server> parseFile(const std::string &path) const;
};

} // namespace webserv
