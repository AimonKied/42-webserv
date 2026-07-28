#include "HttpResponse.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <cctype>
#include <cstdint>
#include <stdexcept>

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

/*
Uebersetzt einen URL-Pfad in einen Pfad auf der Platte und prueft, dass der das Basis-
verzeichnis nicht verlaesst. Alles, was spaeter an dieser Umwandlung dazukommt (Location-
Prefix abschneiden bei root/alias, evtl. Percent-Decoding falls es doch nicht im Parser
landet), gehoert hier rein - und zwar VOR den isPathInsideRoot-Check. Der muss der letzte
Schritt bleiben, sonst wird ein anderer Pfad geprueft als spaeter geoeffnet wird.

baseDir statt LocationConfig als Parameter, damit buildPost spaeter uploadStore
uebergeben kann, ohne dass diese Funktion die Methode kennen muss.
*/
struct ResolvedPath {
    std::string path;
    int errorCode = 0;      // 0 = ok
};

static ResolvedPath resolvePath(const std::string& urlPath, const std::string& baseDir) {
    ResolvedPath result;

    result.path = baseDir;
    if (!result.path.empty() && result.path.back() == '/')
        result.path.pop_back();
    result.path += urlPath;

    if (!isPathInsideRoot(result.path, baseDir)) {
        result.path.clear();
        result.errorCode = 403;
    }
    return result;
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

    /*
    Ein fehlgeschlagenes open() heisst nicht automatisch "gibt es nicht". status() liefert
    not_found, wenn der Pfad wirklich fehlt, und none (mit gesetztem ec), wenn schon das
    Durchlaufen der Elternverzeichnisse an fehlenden Rechten scheitert. Nur der erste Fall
    ist ein 404 - alles andere ist ein Rechteproblem und damit 403, sonst verraet der Server
    ueber den Statuscode, welche Dateien es gibt und welche nicht.
    */
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        const bool notFound = (status.type() == std::filesystem::file_type::not_found);
        return makeErrorResponse(notFound ? 404 : 403, loc);
    }

    /*
    Vorher lief das ueber einen stringstream: erst rdbuf() reinschieben, dann .str()
    rauskopieren. Das sind zwei Puffer zusaetzlich zum Body, also die dreifache Dateigroesse
    im Speicher - bei einer 144-MB-Datei gemessene 433 MB. Mit file_size vorab wird der Body
    genau einmal alloziert und direkt befuellt.

    Das catch ist keine Vorsicht, sondern Pflicht: laut Subject gilt ein Absturz - ausdruecklich
    auch bei Speichermangel - als nicht funktionsfaehiges Projekt. Ohne catch fliegt bad_alloc
    ungefangen bis aus main raus und der Prozess terminiert mit SIGABRT.
    */
    const std::uintmax_t fileSize = std::filesystem::file_size(filePath, ec);
    if (ec)
        return makeErrorResponse(500, loc);

    try {
        res.body.resize(static_cast<size_t>(fileSize));
    } catch (const std::exception&) {
        return makeErrorResponse(500, loc);
    }

    if (fileSize > 0) {
        file.read(&res.body[0], static_cast<std::streamsize>(fileSize));
        if (file.bad())
            return makeErrorResponse(500, loc);
        // Datei kann zwischen file_size und read geschrumpft sein - auf das kuerzen,
        // was wirklich gelesen wurde, sonst haengen Nullbytes hinten dran.
        res.body.resize(static_cast<size_t>(file.gcount()));
    }

    res.statusCode = 200;
    res.headers["Content-Type"] = getMimeType(filePath);
    res.statusText = "OK";
    res.headers["Content-Length"] = std::to_string(res.body.size());
    return res;
};

/*
extension() statt rfind('.') auf dem ganzen Pfad: bei "/dir.css/datei" haette rfind den
Punkt im Verzeichnisnamen gefunden und ".css/datei" als Endung geliefert. Ausserdem wird
kleingeschrieben, sonst landen ".HTML" oder ".PNG" im octet-stream-Fallback und der
Browser laedt die Datei runter statt sie anzuzeigen.
*/
std::string Response::getMimeType(const std::string& filePath) {
    std::string ext = std::filesystem::path(filePath).extension().string();
    if (ext.empty())
        return "application/octet-stream";
    for (char& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

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
    ResolvedPath target = resolvePath(req.path, loc.root);
    if (target.errorCode != 0)
        return makeErrorResponse(target.errorCode, loc);

    std::string fullPath = target.path;
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

    ResolvedPath target = resolvePath(req.path, loc.root);
    if (target.errorCode != 0)
        return makeErrorResponse(target.errorCode, loc);

    const std::string& fullPath = target.path;
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

    ResolvedPath target = resolvePath(req.path, loc.root);
    if (target.errorCode != 0)
        return makeErrorResponse(target.errorCode, loc);

    const std::string& fullPath = target.path;
    std::error_code ec;
    bool existed = std::filesystem::exists(fullPath, ec);
    if (!existed)
        return makeErrorResponse(404, loc);
    if (std::filesystem::is_directory(fullPath, ec))
        return makeErrorResponse(403, loc);
    std::filesystem::remove(fullPath, ec);
    if (ec)
        return makeErrorResponse(500, loc);
    /*
    Bewusst ohne Content-Length: RFC 7230 verbietet den Header bei 204, weil die Antwort
    per Definition keinen Body haben kann. Ein "Content-Length: 0" ist zwar harmlos, aber
    manche Clients werten den Widerspruch als Framing-Fehler.
    */
    Response res;
    res.statusCode = 204;
    res.statusText = "No Content";
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
