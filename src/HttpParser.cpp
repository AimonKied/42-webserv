#include "HttpParser.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>

namespace {

const std::string::size_type MAX_REQUEST_LINE_SIZE = 8192;
const std::string::size_type MAX_HEADERS_SIZE = 16384;

Request errorRequest(int errorCode) {
    Request request;
    request.errorCode = errorCode;
    return request;
}

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

std::string toLower(const std::string &value) {
    std::string result = value;
    for (std::string::size_type index = 0; index < result.size(); ++index) {
        if (result[index] >= 'A' && result[index] <= 'Z') {
            result[index] =
                static_cast<char>(result[index] - 'A' + 'a');
        }
    }
    return result;
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

        const std::string name = toLower(trim(headerLine.substr(0, colon)));
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
    std::string::size_type contentLength = 0;
    if (value.empty()) {
        throw std::invalid_argument("Invalid Content-Length header");
    }

    for (std::string::size_type index = 0; index < value.size(); ++index) {
        if (value[index] < '0' || value[index] > '9') {
            throw std::invalid_argument("Invalid Content-Length header");
        }

        const std::string::size_type digit =
            static_cast<std::string::size_type>(value[index] - '0');
        if (contentLength >
            (std::numeric_limits<std::string::size_type>::max() - digit) / 10) {
            throw std::invalid_argument("Invalid Content-Length header");
        }
        contentLength = contentLength * 10 + digit;
    }
    return contentLength;
}

std::string::size_type parseChunkSize(const std::string &line) {
    const std::string::size_type extensionStart = line.find(';');
    const std::string value = trim(line.substr(0, extensionStart));
    std::string::size_type chunkSize = 0;

    if (value.empty()) {
        throw std::invalid_argument("Invalid chunk size");
    }

    for (std::string::size_type index = 0; index < value.size(); ++index) {
        std::string::size_type digit = 0;
        if (value[index] >= '0' && value[index] <= '9') {
            digit = static_cast<std::string::size_type>(value[index] - '0');
        } else if (value[index] >= 'a' && value[index] <= 'f') {
            digit = static_cast<std::string::size_type>(value[index] - 'a' + 10);
        } else if (value[index] >= 'A' && value[index] <= 'F') {
            digit = static_cast<std::string::size_type>(value[index] - 'A' + 10);
        } else {
            throw std::invalid_argument("Invalid chunk size");
        }

        if (chunkSize >
            (std::numeric_limits<std::string::size_type>::max() - digit) / 16) {
            throw std::invalid_argument("Invalid chunk size");
        }
        chunkSize = chunkSize * 16 + digit;
    }
    return chunkSize;
}

bool parseChunkedBody(const std::string &rawRequest,
                      std::string::size_type bodyStart,
                      std::string::size_type maxBodySize,
                      std::string &body,
                      int &errorCode) {
    std::string::size_type current = bodyStart;

    while (true) {
        const std::string::size_type sizeLineEnd =
            rawRequest.find("\r\n", current);
        if (sizeLineEnd == std::string::npos) {
            return false;
        }

        std::string::size_type chunkSize = 0;
        try {
            chunkSize = parseChunkSize(
                rawRequest.substr(current, sizeLineEnd - current));
        } catch (const std::invalid_argument &) {
            errorCode = 400;
            return false;
        }
        current = sizeLineEnd + 2;

        if (chunkSize == 0) {
            if (rawRequest.size() - current < 2) {
                return false;
            }
            if (rawRequest.compare(current, 2, "\r\n") != 0) {
                errorCode = 400;
                return false;
            }
            return true;
        }

        if (chunkSize > maxBodySize - body.size()) {
            errorCode = 413;
            return false;
        }
        if (chunkSize > rawRequest.size() - current) {
            return false;
        }
        if (rawRequest.size() - (current + chunkSize) < 2) {
            return false;
        }

        body.append(rawRequest, current, chunkSize);
        current += chunkSize;
        if (rawRequest.compare(current, 2, "\r\n") != 0) {
            errorCode = 400;
            return false;
        }
        current += 2;
    }
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

Request HttpParser::parse(const std::string &rawRequest,
                          std::size_t maxBodySize) const {
    const std::string::size_type lineEnd = rawRequest.find("\r\n");
    if (lineEnd == std::string::npos) {
        if (rawRequest.size() > MAX_REQUEST_LINE_SIZE) {
            return errorRequest(414);
        }
        return Request();
    }
    if (lineEnd > MAX_REQUEST_LINE_SIZE) {
        return errorRequest(414);
    }

    const std::string requestLine = rawRequest.substr(0, lineEnd);
    std::istringstream stream(requestLine);

    std::string methodToken;
    std::string target;
    std::string version;
    std::string extra;

    if (!(stream >> methodToken >> target >> version) || (stream >> extra)) {
        return errorRequest(400);
    }

    Request request;
    request.method = parseMethod(methodToken);
    if (request.method == Method::UNKNOWN) {
        return errorRequest(405);
    }

    if (version != "HTTP/1.1") {
        return errorRequest(505);
    }

    if (target.empty() || target[0] != '/' ||
        target.find('#') != std::string::npos) {
        return errorRequest(400);
    }

    request.uri = target;
    const std::string::size_type queryStart = target.find('?');
    request.path = target.substr(0, queryStart);
    if (queryStart != std::string::npos) {
        request.query = target.substr(queryStart + 1);
    }
    request.version = version;

    const std::string::size_type headersStart = lineEnd + 2;
    const std::string::size_type headersEnd =
        rawRequest.find("\r\n\r\n", headersStart);
    if (headersEnd == std::string::npos) {
        if (rawRequest.size() - headersStart > MAX_HEADERS_SIZE) {
            return errorRequest(431);
        }
        return request;
    }
    if (headersEnd - headersStart > MAX_HEADERS_SIZE) {
        return errorRequest(431);
    }

    std::string::size_type bodyStart = 0;
    try {
        bodyStart = parseHeaders(rawRequest, headersStart, request);
    } catch (const std::invalid_argument &) {
        return errorRequest(400);
    }

    if (!hasHeader(request, "host") || request.headers["host"].empty()) {
        return errorRequest(400);
    }

    const std::unordered_map<std::string, std::string>::const_iterator contentLengthIt =
        request.headers.find("content-length");
    const std::unordered_map<std::string, std::string>::const_iterator transferEncodingIt =
        request.headers.find("transfer-encoding");

    if (contentLengthIt != request.headers.end() &&
        transferEncodingIt != request.headers.end()) {
        return errorRequest(400);
    }

    if (transferEncodingIt != request.headers.end()) {
        if (toLower(trim(transferEncodingIt->second)) != "chunked") {
            return errorRequest(501);
        }

        int chunkError = 0;
        if (!parseChunkedBody(rawRequest, bodyStart, maxBodySize,
                              request.body, chunkError)) {
            if (chunkError != 0) {
                return errorRequest(chunkError);
            }
            return request;
        }
    }

    if (contentLengthIt != request.headers.end()) {
        std::string::size_type contentLength = 0;
        try {
            contentLength = parseContentLength(contentLengthIt->second);
        } catch (const std::invalid_argument &) {
            return errorRequest(400);
        }

        if (contentLength > maxBodySize) {
            return errorRequest(413);
        }
        if (contentLength > rawRequest.size() - bodyStart) {
            return request;
        }

        request.body = rawRequest.substr(bodyStart, contentLength);
    }

    request.complete = true;
    return request;
}
