#include "HttpResponse.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
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

Response Response::buildGet(const Request& req, const std::string& rootDir) {
    if (req.path.find("..") != std::string::npos) {
        Response res;
        res.statusCode = 403;
        res.statusText = "Forbidden";
        res.body = "403 Forbidden";
        res.headers["Content-Length"] = std::to_string(res.body.size());
        return res;
    }
    std::string fullPath = rootDir;
    if (!fullPath.empty() && fullPath.back() == '/')
        fullPath.pop_back();
    fullPath += req.path;

    std::error_code ec;
    if (std::filesystem::is_directory(fullPath, ec))
        fullPath += "/index.html";
    return serveFile(fullPath);
}

Response Response::buildPost(const Request& req, const std::string& rootDir) {
    if (req.path.find("..") != std::string::npos) {
        Response res;
        res.statusCode = 403;
        res.statusText = "Forbidden";
        res.body = "403 Forbidden";
        res.headers["Content-Length"] = std::to_string(res.body.size());
        return res;
    }
    if (!req.path.empty() && req.path.back() == '/') {
        Response res;
        res.statusCode = 400;
        res.statusText = "Bad Request";
        res.body = "400 Bad Request";
        res.headers["Content-Length"] = std::to_string(res.body.size());
        return res;
    }
    std::string fullPath = rootDir;
    if (!fullPath.empty() && fullPath.back() == '/')
        fullPath.pop_back();
    fullPath += req.path;

    bool existed = std::filesystem::exists(fullPath);
    std::ofstream outFile(fullPath, std::ios::binary);
    if (!outFile.is_open()) {
        Response res;
        res.statusCode = 500;
        res.statusText = "Internal Server Error";
        res.body = "500 Internal Server Error";
        res.headers["Content-Length"] = std::to_string(res.body.size());
        return res;
    }

    outFile << req.body;
    outFile.close();
    Response res;
    res.statusCode = existed ? 200 : 201;
    res.statusText = existed ? "OK" : "Created";
    res.body = "";
    res.headers["Content-Length"] = std::to_string(res.body.size());
    return res;
}

Response Response::buildDelete(const Request& req, const std::string& rootDir) {
    return Response();
}
