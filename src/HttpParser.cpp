#include "HttpParser.hpp"

#include <sstream>
#include <stdexcept>

namespace webserv {

Request::Request()
    : method(HttpMethod::Unknown), path(), version(), headers(), body() {
}

HttpParser::HttpParser() {
}

HttpParser::~HttpParser() {
}

HttpMethod HttpParser::parseMethod(const std::string &method) const {
    if (method == "GET") {
        return HttpMethod::Get;
    }
    if (method == "POST") {
        return HttpMethod::Post;
    }
    if (method == "DELETE") {
        return HttpMethod::Delete;
    }
    return HttpMethod::Unknown;
}

Request HttpParser::parse(const std::string &rawRequest) const {
    const std::string::size_type lineEnd = rawRequest.find("\r\n");
    if (lineEnd == std::string::npos) {
        throw std::invalid_argument("Incomplete request line");
    }

    const std::string requestLine = rawRequest.substr(0, lineEnd);
    std::istringstream stream(requestLine);

    std::string methodToken;
    std::string target;
    std::string version;
    std::string extra;

    if (!(stream >> methodToken >> target >> version) || (stream >> extra)) {
        throw std::invalid_argument("Malformed request line");
    }

    Request request;
    request.method = parseMethod(methodToken);
    if (request.method == HttpMethod::Unknown) {
        throw std::invalid_argument("Unsupported method");
    }

    if (version != "HTTP/1.1") {
        throw std::invalid_argument("Unsupported HTTP version");
    }

    request.path = target;
    request.version = version;
    return request;
}

} // namespace webserv
