#pragma once

#include "HttpRequest.hpp"
#include "ConfigTypes.hpp"
#include <string>

struct CgiMatch {
    bool isCgi = false;
    std::string scriptName;
    std::string pathInfo;
};

CgiMatch resolveCgiTarget(const Request& req, const LocationConfig& loc);
