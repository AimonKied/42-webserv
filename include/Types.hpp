#pragma once

#include <map>
#include <string>
#include <vector>

namespace webserv {

enum class HttpMethod {
    Get,
    Post,
    Put,
    Delete,
    Head,
    Options,
    Unknown
};

enum class HttpStatus {
    Ok = 200,
    NotFound = 404,
    InternalServerError = 500,
    BadRequest = 400
};

using HeaderMap = std::map<std::string, std::string>;
using StringList = std::vector<std::string>;

} // namespace webserv
