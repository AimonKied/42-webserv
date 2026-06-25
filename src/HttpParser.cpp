#include "HttpParser.hpp"

namespace webserv {

HttpParser::HttpParser() {
}

HttpParser::~HttpParser() {
}

Request HttpParser::parse(const std::string &rawRequest) const {
    (void)rawRequest;
    // TODO: parse raw HTTP request into Request object
    Request request;
    return request;
}

} // namespace webserv
