#pragma once

#include "HttpRequest.hpp"
#include <string>

class HttpParser {
public:
    HttpParser();
    ~HttpParser();

    Request parse(const std::string &rawRequest) const;

private:
    Method parseMethod(const std::string &method) const;
};
