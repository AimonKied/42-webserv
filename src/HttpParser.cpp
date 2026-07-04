#include "HttpParser.hpp"

#include <sstream>
#include <stdexcept>

namespace webserv {

namespace {

std::string trim(const std::string &value) {
    std::string::size_type start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t')) {
        ++start;
    }

    std::string::size_type end = value.size();
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t')) {
        --end;
    }

    return value.substr(start, end - start);
}

void parseHeaders(const std::string &rawRequest,
                  std::string::size_type headersStart,
                  Request &request) {
    std::string::size_type current = headersStart;

    while (current <= rawRequest.size()) {
        const std::string::size_type lineEnd = rawRequest.find("\r\n", current);
        if (lineEnd == std::string::npos) {
            throw std::invalid_argument("Incomplete header line");
        }

        if (lineEnd == current) {
            return;
        }

        const std::string headerLine = rawRequest.substr(current, lineEnd - current);
        const std::string::size_type colon = headerLine.find(':');
        if (colon == std::string::npos) {
            throw std::invalid_argument("Malformed header line");
        }

        const std::string name = trim(headerLine.substr(0, colon));
        const std::string value = trim(headerLine.substr(colon + 1));
        if (name.empty()) {
            throw std::invalid_argument("Malformed header line");
        }

        request.headers[name] = value;
        current = lineEnd + 2;
    }
}

} // namespace

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

    parseHeaders(rawRequest, lineEnd + 2, request);

    return request;
}

} // namespace webserv
