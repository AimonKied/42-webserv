#include "HttpResponse.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>

Response makeErrorResponse(int code, const LocationConfig& loc);
std::string getStatusText(int code);

/*
weakly_canonical macht aus rootDir und fullPath absolute Pfade und die einzelnen Segmente
der Pfade werden dann in einem loop verglichen. Solange alle segmente von root im zielpfad
vorkommen (in gleicher reihenfolge, am anfang), wird true zurueckgegeben.
*/
static bool isPathInsideRoot(const std::string& fullPath, const std::string& rootDir) {
    std::error_code ec;

    std::filesystem::path resolvedRoot = std::filesystem::weakly_canonical(rootDir, ec);
    if (ec)
        return false;
    std::filesystem::path resolvedFull = std::filesystem::weakly_canonical(fullPath, ec);
    if (ec)
        return false;

    auto rootSegment = resolvedRoot.begin(); auto fullSegment = resolvedFull.begin();

    while (rootSegment != resolvedRoot.end()) {
        bool fullPathRanOut = (fullSegment == resolvedFull.end());
        bool segmentsDiffer = !fullPathRanOut && (*fullSegment != *rootSegment);

        if (fullPathRanOut || segmentsDiffer)
            return false;

        ++rootSegment;
        ++fullSegment;
    }

    return true;
}

std::string Response::toString() const {
    std::string result;

    result += "HTTP/1.1 " + std::to_string(statusCode) + " " + statusText + "\r\n";
    for (const auto& header : headers) {
        result += header.first + ": " + header.second + "\r\n";
    }
    result += "\r\n" + body;
    return result;
};

/*
Ein ifstream laesst sich auch auf ein Verzeichnis oeffnen - is_open() ist dann true und
das Lesen liefert einfach 0 Bytes. Ohne diesen Check kaeme bei jedem GET auf ein
Verzeichnis, dessen index-Datei nicht aufgeloest werden konnte, ein 200 mit leerem Body
raus statt eines Fehlers. Gleiches gilt fuer FIFOs und Geraetedateien im Document-Root.
403 statt 404, weil die Ressource ja existiert - sobald autoindex implementiert ist,
faengt buildGet den Verzeichnisfall vorher ab und das hier bleibt das Sicherheitsnetz.
*/
Response Response::serveFile(const std::string& filePath, const LocationConfig& loc) {
    Response res;
    std::error_code ec;

    const std::filesystem::file_status status = std::filesystem::status(filePath, ec);
    if (std::filesystem::exists(status) && !std::filesystem::is_regular_file(status))
        return makeErrorResponse(403, loc);

    std::ifstream file(filePath);
    if (!file.is_open())
        return makeErrorResponse(404, loc);
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
    size_t dotPos = filePath.rfind('.');
    if (dotPos == std::string::npos)
        return "application/octet-stream";
    std::string ext = filePath.substr(dotPos);

    if (ext == ".html") return "text/html";
    else if (ext == ".css") return "text/css";
    else if (ext == ".js") return "application/javascript";
    else if (ext == ".png") return "image/png";
    else if (ext == ".jpg") return "image/jpeg";
    else if (ext == ".txt") return "text/plain";
    else if (ext == ".gif") return "image/gif";
    else if (ext == ".svg") return "image/svg+xml";
    else if (ext == ".ico") return "image/x-icon";
    else if (ext == ".json") return "application/json";
    else if (ext == ".xml") return "application/xml";
    else if (ext == ".pdf") return "application/pdf";
    else if (ext == ".woff") return "font/woff";
    else if (ext == ".woff2") return "font/woff2";
    else if (ext == ".mp4") return "video/mp4";
    return "application/octet-stream";
};

/*
Einstieg fuer den Server-Loop. Der Parser meldet Fehler nicht per Exception, sondern
ueber Request::errorCode (400/405/413/414/431/501/505) - der hat Vorrang vor allem anderen,
sonst wuerde ein kaputter Request hier als normaler GET behandelt.
Ein unvollstaendiger Request ohne Fehlercode heisst "weiterlesen" und darf gar nicht
erst ankommen; das 400 hier ist nur ein Netz, falls der Server-Loop zu frueh antwortet.
*/
Response Response::build(const Request& req, const LocationConfig& loc) {
    if (req.errorCode != 0)
        return makeErrorResponse(req.errorCode, loc);
    if (!req.complete)
        return makeErrorResponse(400, loc);

    switch(req.method) {
        case Method::GET:
            return buildGet(req, loc);
        case Method::POST:
            return buildPost(req, loc);
        case Method::DELETE:
            return buildDelete(req, loc);
        default:
            return makeErrorResponse(405, loc);
    }
}

Response Response::buildGet(const Request& req, const LocationConfig& loc) {
    std::string fullPath = loc.root;
    if (!fullPath.empty() && fullPath.back() == '/')
        fullPath.pop_back();
    fullPath += req.path;

    if (!isPathInsideRoot(fullPath, loc.root))
        return makeErrorResponse(403, loc);

    std::error_code ec;
    if (std::filesystem::is_directory(fullPath, ec)) {
        if (req.path.empty() || req.path.back() != '/') {
            std::string location = req.path + "/";
            if (!req.query.empty())
                location += "?" + req.query;
            return buildRedirect(301, location);
        }
        fullPath += loc.index;
    }
    return serveFile(fullPath, loc);
}

Response Response::buildPost(const Request& req, const LocationConfig& loc) {
    if (!req.path.empty() && req.path.back() == '/')
        return makeErrorResponse(400, loc);
    std::string fullPath = loc.root;
    if (!fullPath.empty() && fullPath.back() == '/')
        fullPath.pop_back();
    fullPath += req.path;

    if (!isPathInsideRoot(fullPath, loc.root))
        return makeErrorResponse(403, loc);

    std::error_code ec;
    bool existed = std::filesystem::exists(fullPath, ec);
    std::ofstream outFile(fullPath, std::ios::binary);
    if (!outFile.is_open())
        return makeErrorResponse(500, loc);

    outFile << req.body;
    outFile.close();
    Response res;
    res.statusCode = existed ? 200 : 201;
    res.statusText = existed ? "OK" : "Created";
    res.body = "";
    res.headers["Content-Length"] = std::to_string(res.body.size());
    return res;
}

Response Response::buildDelete(const Request& req, const LocationConfig& loc) {
    if (!req.path.empty() && req.path.back() == '/')
        return makeErrorResponse(400, loc);
    std::string fullPath = loc.root;
    if (!fullPath.empty() && fullPath.back() == '/')
        fullPath.pop_back();
    fullPath += req.path;

    if (!isPathInsideRoot(fullPath, loc.root))
        return makeErrorResponse(403, loc);

    std::error_code ec;
    bool existed = std::filesystem::exists(fullPath, ec);
    if (!existed)
        return makeErrorResponse(404, loc);
    if (std::filesystem::is_directory(fullPath, ec))
        return makeErrorResponse(403, loc);
    std::filesystem::remove(fullPath, ec);
    if (ec)
        return makeErrorResponse(500, loc);
    Response res;
    res.statusCode = 204;
    res.statusText = "No Content";
    res.headers["Content-Length"] = std::to_string(res.body.size());
    return res;
}

Response Response::buildRedirect(int code, const std::string& location) {
    Response res;
    res.statusCode = code;
    res.statusText = getStatusText(code);
    res.headers["Location"] = location;
    res.body = "";
    res.headers["Content-Length"] = std::to_string(res.body.size());
    return res;
}
