#include "HttpParser.hpp"

#include <sstream>
#include <stdexcept>

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

std::string::size_type parseHeaders(const std::string &rawRequest,
                                    std::string::size_type headersStart,
                                    Request &request) {
    std::string::size_type current = headersStart;

    while (current <= rawRequest.size()) {
        const std::string::size_type lineEnd = rawRequest.find("\r\n", current);
        if (lineEnd == std::string::npos) {
            throw std::invalid_argument("Incomplete header line");
        }

        if (lineEnd == current) {
            return current + 2;
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

        if (request.headers.find(name) != request.headers.end()) {
            throw std::invalid_argument("Duplicate header line");
        }

        request.headers[name] = value;
        current = lineEnd + 2;
    }

    return current;
}

bool hasHeader(const Request &request, const std::string &name) {
    return request.headers.find(name) != request.headers.end();
}

std::string::size_type parseContentLength(const std::string &value) {
    std::istringstream lengthStream(value);
    std::string::size_type contentLength = 0;
    char leftover = '\0';

    if (!(lengthStream >> contentLength) || (lengthStream >> leftover)) {
        throw std::invalid_argument("Invalid Content-Length header");
    }

    return contentLength;
}

} // namespace

Request::Request()
    : method(Method::UNKNOWN),
      uri(),
      path(),
      query(),
      version(),
      headers(),
      body(),
      complete(false),
      errorCode(0) {
}

HttpParser::HttpParser() {
}

HttpParser::~HttpParser() {
}

Method HttpParser::parseMethod(const std::string &method) const {
    if (method == "GET") {
        return Method::GET;
    }
    if (method == "POST") {
        return Method::POST;
    }
    if (method == "DELETE") {
        return Method::DELETE;
    }
    return Method::UNKNOWN;
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
    if (request.method == Method::UNKNOWN) {
        throw std::invalid_argument("Unsupported method");
    }

    if (version != "HTTP/1.1") {
        throw std::invalid_argument("Unsupported HTTP version");
    }

    request.uri = target;
    const std::string::size_type queryStart = target.find('?');
    request.path = target.substr(0, queryStart);
    if (queryStart != std::string::npos) {
        request.query = target.substr(queryStart + 1);
    }
    request.version = version;

    const std::string::size_type bodyStart = parseHeaders(rawRequest, lineEnd + 2, request);

    if (!hasHeader(request, "Host")) {
        throw std::invalid_argument("Missing Host header");
    }

    const std::unordered_map<std::string, std::string>::const_iterator contentLengthIt =
        request.headers.find("Content-Length");
    if (contentLengthIt != request.headers.end()) {
        const std::string::size_type contentLength = parseContentLength(contentLengthIt->second);

        if (rawRequest.size() < bodyStart + contentLength) {
            throw std::invalid_argument("Incomplete body");
        }

        request.body = rawRequest.substr(bodyStart, contentLength);
    }

    request.complete = true;
    return request;
}
