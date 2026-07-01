#pragma once

#include <string>
#include <unordered_map>

class Response {
    public:
        int statusCode;
        std::string statusText;
        std::string body;

        std::unordered_map<std::string, std::string> headers;
};