#pragma once

#include "Types.hpp"
#include <string>

namespace webserv {

class Response {
public:
    Response();
    ~Response();

    HttpStatus status;
    HeaderMap headers;
    std::string body;

    std::string serialize() const;
};


} // namespace webserv

std::string build_response_HARDCODED(int client_fd);
