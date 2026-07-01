#pragma once

#include <map>
#include <string>

namespace webserv {

enum class HttpMethod {
    Get,
    Post,
    Delete,
    Unknown
};

struct Request {
    HttpMethod method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;

    Request();
};

} // namespace webserv