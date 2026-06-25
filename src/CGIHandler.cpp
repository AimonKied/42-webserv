#include "CGIHandler.hpp"

namespace webserv {

CGIHandler::CGIHandler() {
}

CGIHandler::~CGIHandler() {
}

Response CGIHandler::execute(const Request &request, const std::string &scriptPath) const {
    (void)request;
    (void)scriptPath;
    // TODO: execute CGI script and capture stdout/stderr
    Response response;
    return response;
}

} // namespace webserv
