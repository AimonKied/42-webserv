#pragma once

#include "HttpRequest.hpp"
#include "ConfigTypes.hpp"
#include <string>
#include <unordered_map>

class Response {
    public:
        int statusCode = 200;
        std::string statusText = "OK";
        std::string body;

        std::unordered_map<std::string, std::string> headers;

        std::string toString() const;
        static Response serveFile(const std::string& filePath, const LocationConfig&);
        static std::string getMimeType(const std::string& filePath);
        static Response build(const Request& req, const LocationConfig&);
        static Response buildGet(const Request& req, const LocationConfig&);
        static Response buildPost(const Request& req, const LocationConfig&);
        static Response buildDelete(const Request& req, const LocationConfig&);
        static Response buildRedirect(int code, const std::string& location);
};
