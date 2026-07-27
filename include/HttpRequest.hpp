#pragma once

#include <string>
#include <unordered_map>

enum class Method {
    GET,
    POST,
    DELETE,
    UNKNOWN
};

class Request {
public:
    Request();

    Method method;
    std::string uri;
    std::string path;
    std::string query;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    bool complete;
    int errorCode;
};
