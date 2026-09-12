#include "HttpResponse.hpp"
#include <filesystem>

Response makeErrorResponse(int code, const LocationConfig& loc);

Response buildDirectoryListing(const std::string& fullPath, const std::string& urlPath, const LocationConfig& loc) {
    Response res;

    std::error_code ec;
    std::filesystem::directory_iterator it(fullPath, ec);
    if (ec) {
        int errorCode;
        if (ec == std::errc::permission_denied)
            errorCode = 403;
        else
            errorCode = 500;
        return makeErrorResponse(errorCode, loc);
    }

    res.body = "<html><head><title>Index of " + urlPath + "</title></head><body><h1>Index of " + urlPath + "</h1><ul>";
    for (const auto& entry : it) {
        if (entry.is_directory()) {
            res.body += "<li><a href=\"" + urlPath + entry.path().filename().string() + "/\">" + entry.path().filename().string() + "/</a></li>";
        } else {
            res.body += "<li><a href=\"" + urlPath + entry.path().filename().string() + "\">" + entry.path().filename().string() + "</a></li>";
        }
    }
    res.body += "</ul></body></html>";
    res.headers["Content-Type"] = "text/html";
    res.headers["Content-Length"] = std::to_string(res.body.size());
    return res;
}
