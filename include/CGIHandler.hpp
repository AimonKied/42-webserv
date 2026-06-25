#pragma once

#include "Request.hpp"
#include "Response.hpp"
#include <string>

namespace webserv {

class CGIHandler {
public:
    CGIHandler();
    ~CGIHandler();

    Response execute(const Request &request, const std::string &scriptPath) const;
};

} // namespace webserv
