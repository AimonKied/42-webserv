#include "HttpResponse.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>

std::string getStatusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

Response makeErrorResponse(int code) {
    Response res;
    res.statusCode = code;
    res.statusText = getStatusText(code);
    res.body = std::to_string(code) + " " + res.statusText;
    res.headers["Content-Length"] = std::to_string(res.body.size());
    return res;
}

