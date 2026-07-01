#pragma once

#include "Request.hpp"
#include <string>

namespace webserv {

class HttpParser {
public:
    HttpParser();
    ~HttpParser();

    Request parse(const std::string &rawRequest) const;

private:
    HttpMethod parseMethod(const std::string &method) const;
};

} // namespace webserv