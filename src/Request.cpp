#include "Request.hpp"

namespace webserv {

Request::Request()
    : method(HttpMethod::Unknown), path(), version(), headers(), body() {
}

Request::~Request() {
}

} // namespace webserv
