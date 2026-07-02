#include "HttpResponse.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>

Response makeErrorResponse(int code);

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

    if (!file.is_open())
        return makeErrorResponse(404);
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
            return makeErrorResponse(405);
    }
}

Response Response::buildGet(const Request& req, const std::string& rootDir) {
    if (req.path.find("..") != std::string::npos)
        return makeErrorResponse(403);
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
    if (req.path.find("..") != std::string::npos)
        return makeErrorResponse(403);
    if (!req.path.empty() && req.path.back() == '/')
        return makeErrorResponse(400);
    std::string fullPath = rootDir;
    if (!fullPath.empty() && fullPath.back() == '/')
        fullPath.pop_back();
    fullPath += req.path;

    bool existed = std::filesystem::exists(fullPath);
    std::ofstream outFile(fullPath, std::ios::binary);
    if (!outFile.is_open())
        return makeErrorResponse(500);

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
        if (req.path.find("..") != std::string::npos)
            return makeErrorResponse(403);
        if (!req.path.empty() && req.path.back() == '/')
            return makeErrorResponse(400);
        std::string fullPath = rootDir;
        if (!fullPath.empty() && fullPath.back() == '/')
            fullPath.pop_back();
        fullPath += req.path;
        bool existed = std::filesystem::exists(fullPath);
        if (!existed)
            return makeErrorResponse(404);
        std::error_code ec;
        if (std::filesystem::is_directory(fullPath, ec))
            return makeErrorResponse(403);
        std::filesystem::remove(fullPath, ec);
        if (ec)
            return makeErrorResponse(500);
        Response res;
        res.statusCode = 204;
        res.statusText = "No Content";
        res.headers["Content-Length"] = std::to_string(res.body.size());
        return res;
}
