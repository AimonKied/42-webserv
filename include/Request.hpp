#pragma once

#include "Types.hpp"
#include <string>

namespace webserv {

class Request {
public:
    Request();
    ~Request();

    HttpMethod method;
    std::string path;
    std::string version;
    HeaderMap headers;
    std::string body;
};

} // namespace webserv
