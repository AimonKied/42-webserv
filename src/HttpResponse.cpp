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

    Response Response::serveFile(const std::string& filePath) {
        Response res;
        std::ifstream file(filePath);

        if (!file.is_open()) {
            res.statusCode = 404;
            res.statusText = "Not Found";
            res.body = "404 Not Found";
            res.headers["Content-Length"] = std::to_string(res.body.size());
            return res;
        }
        std::stringstream ss;
        ss << file.rdbuf();
        res.body = ss.str();
        res.statusCode = 200;
        res.statusText = "OK";
        res.headers["Content-Length"] = std::to_string(res.body.size());
        return res;
};