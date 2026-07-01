#include "HttpResponse.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <string>

std::string Response::toString() const {
    std::string result;

    result += "HTTP/1.1 " + std::to_string(statusCode) + " " + statusText + "\r\n";
    for (const auto& header : headers) {
        result += header.first + ": " + header.second + "\r\n";
    }
    result += "\r\n" + body;
    return result;
};