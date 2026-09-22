#pragma once

#include "HttpRequest.hpp"
#include "ConfigTypes.hpp"
#include <string>
#include <vector>

struct CgiMatch {
    bool isCgi = false;
    std::string scriptName;
    std::string pathInfo;
};

CgiMatch resolveCgiTarget(const Request& req, const LocationConfig& loc);

std::vector<std::string> buildCgiEnv(const Request& req,
                                     const CgiMatch& match,
                                     const ServerConfig& server,
                                     const std::string& scriptPath);
