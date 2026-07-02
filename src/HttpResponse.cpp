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
    res.headers["Content-Type"] = getMimeType(filePath);
    res.statusText = "OK";
    res.headers["Content-Length"] = std::to_string(res.body.size());
    return res;
};

std::string Response::getMimeType(const std::string& filePath) {
    std::string ext = filePath.substr(filePath.rfind('.'));

    if (ext == ".html")
        return "text/html";
    else if (ext == ".css")
        return "text/css";
    else if (ext == ".js")
        return ("application/javascript");
    else if (ext == ".png")
        return "image/png";
    else if (ext == ".jpg")
        return "image/jpeg";
    else if (ext == ".txt")
        return ("text/plain");
    return "application/octet-stream";
};

Response Response::build(const Request& req, const std::string& rootDir) {
    switch(req.method) {
        case Method::GET:
            return buildGet(req, rootDir);
        case Method::POST:
            return buildPost(req,rootDir);
        case Method::DELETE:
            return buildDelete(req, rootDir);
        default:
            Response res;
            res.statusCode = 405;
            res.statusText = "Method Not Allowed";
            res.body = "405 Method Not Allowed";
            res.headers["Content-Length"] = std::to_string(res.body.size());
            return res;
    }
}