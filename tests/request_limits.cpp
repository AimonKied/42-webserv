#include "HttpParser.hpp"
#include <cassert>
#include <iostream>
#include <limits>
#include <sstream>

static std::string fixedHeader(size_t size) {
    return "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: " +
        std::to_string(size) + "\r\n\r\n";
}

static std::string chunkHeader() {
    return "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n";
}

static std::string chunk(size_t size) {
    std::ostringstream hex;
    hex << std::hex << size;
    return hex.str() + "\r\n" + std::string(size, 'x') + "\r\n";
}

int main() {
    const HttpParser parser;
    const size_t limit = 1024 * 1024;
    for (size_t size : {size_t(0), limit - 1, limit}) {
        Request request = parser.parse(fixedHeader(size) + std::string(size, 'x'), limit);
        assert(request.complete && !request.errorCode && request.body.size() == size);
    }
    assert(parser.parse(fixedHeader(limit + 1), limit).errorCode == 413);
    assert(!parser.parse(fixedHeader(limit) + std::string(limit - 1, 'x'), limit).complete);
    assert(parser.parse(fixedHeader(2 * limit) + std::string(2 * limit, 'x'), 2 * limit).complete);
    assert(parser.parse(fixedHeader(0), 0).complete);
    assert(parser.parse(fixedHeader(1), 0).errorCode == 413);
    std::cout << "PASS: Content-Length boundaries, zero and configured sizes above one MiB\n";

    Request request = parser.parse(chunkHeader() + chunk(limit) + "0\r\n\r\n", limit);
    assert(request.complete && !request.errorCode && request.body.size() == limit);
    assert(parser.parse(chunkHeader() + chunk(limit) + "1\r\n", limit).errorCode == 413);
    assert(!parser.parse(chunkHeader() + chunk(limit) + "0\r\n\r", limit).complete);
    assert(parser.parse(chunkHeader() + "0\r\n\r\n", 0).complete);
    assert(parser.parse(chunkHeader() + "1\r\n", 0).errorCode == 413);
    // Full framing can exceed the body by a factor of six without violating it.
    std::string tiny = chunkHeader();
    for (size_t i = 0; i < limit; ++i) tiny += "1\r\nx\r\n";
    tiny += "0\r\n\r\n";
    assert(tiny.size() <= HttpParser::maxBufferedRequestSize(limit));
    request = parser.parse(tiny, limit);
    assert(request.complete && !request.errorCode && request.body.size() == limit);
    std::cout << "PASS: chunked boundaries, partial terminator and one-byte chunks at the body limit\n";

    const std::string extensionChunk = "1;" + std::string(8189, 'a') + "\r\nx\r\n";
    std::string extensions = chunkHeader();
    for (int i = 0; i < 8; ++i) extensions += extensionChunk;
    assert(parser.parse(extensions + "0\r\n\r\n", 16).complete);
    extensions += extensionChunk;
    assert(parser.parse(extensions + "0\r\n\r\n", 16).errorCode == 413);
    assert(parser.parse(chunkHeader() + "1;" + std::string(8191, 'a'), limit).errorCode == 413);
    assert(parser.parse(chunkHeader() + "1;" + std::string(8191, 'a') + "\r\nx\r\n", limit).errorCode == 413);
    assert(parser.parse("GET / HTTP/1.1\r\nHost: localhost\r\nX: " + std::string(17000, 'a') + "\r\n\r\n", 0).errorCode == 431);
    assert(parser.parse("GET /" + std::string(8200, 'a'), 0).errorCode == 414);
    std::cout << "PASS: chunk-metadata bounds and independent header/request-line limits\n";

    const size_t maximum = std::numeric_limits<size_t>::max();
    assert(HttpParser::maxBufferedRequestSize(maximum) == maximum);
    assert(HttpParser::maxBufferedRequestSize(maximum / 5 + 1) == maximum);
    assert(HttpParser::maxBufferedRequestSize(maximum / 6) == maximum);
    assert(HttpParser::maxBufferedRequestSize(0) > 0);
    assert(parser.parse(fixedHeader(0), maximum).complete);
    std::cout << "PASS: raw-buffer budget saturates instead of overflowing\n";
}
