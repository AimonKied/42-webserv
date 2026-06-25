#include "Response.hpp"

namespace webserv {

Response::Response()
    : status(HttpStatus::Ok), headers(), body() {
}

Response::~Response() {
}

std::string Response::serialize() const {
    // TODO: build raw HTTP response string
    return std::string();
}

} // namespace webserv
