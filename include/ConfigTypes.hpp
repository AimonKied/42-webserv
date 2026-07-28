#pragma once

#include "HttpRequest.hpp"
#include <string>
#include <vector>
#include <unordered_map>

struct LocationConfig {
    std::string path;
    std::vector<Method> methods;
    std::string root;
    std::string index;
    bool autoindex = false;
    std::string uploadStore;
    std::string cgiExtension;
    int redirectCode = 0;
    std::string redirectTarget;
};

struct ServerConfig {
    std::string host;
    int port = 0;
    std::string serverName;
    size_t clientMaxBodySize = 0;
    std::unordered_map<int, std::string> errorPages;
    std::vector<LocationConfig> locations;
};

using Config = std::vector<ServerConfig>;