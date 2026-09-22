#include "Cgi.hpp"
#include <filesystem>

CgiMatch resolveCgiTarget(const Request& req, const LocationConfig& loc) {
    CgiMatch result;

    if (loc.cgiExtension.empty())
        return result;

    const std::string extension = std::filesystem::path(req.path).extension().string();
    if (extension != loc.cgiExtension)
        return result;

    result.isCgi = true;
    result.scriptName = req.path;
    return result;
}
