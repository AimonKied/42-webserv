#pragma once

#include "HttpRequest.hpp"
#include <string>
#include <unordered_map>

class Response {
    public:
        int statusCode;
        std::string statusText;
        std::string body;

        std::unordered_map<std::string, std::string> headers;

        std::string toString() const;
        static Response serveFile(const std::string& filePath);
        static std::string getMimeType(const std::string& filePath);
        static Response build(const Request& req, const std::string& rootDir);
};