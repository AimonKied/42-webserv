#pragma once

#include "HttpRequest.hpp"
#include <cstddef>
#include <string>

class HttpParser {
public:
    HttpParser();
    ~HttpParser();

    Request parse(const std::string &rawRequest,
                  std::size_t maxBodySize = 1024 * 1024) const;

private:
    Method parseMethod(const std::string &method) const;
};
