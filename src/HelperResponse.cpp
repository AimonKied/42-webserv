#include "HttpResponse.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>

// 200 OK                          - Request erfolgreich, Body enthaelt angeforderte Ressource
// 201 Created                     - Ressource wurde neu angelegt (z.B. POST)
// 204 No Content                  - Erfolgreich, aber kein Body (z.B. DELETE)
// 301 Moved Permanently           - Ressource dauerhaft umgezogen, Client darf cachen
// 302 Found                       - Ressource temporaer umgezogen, kein Caching
// 307 Temporary Redirect          - wie 302, Methode/Body bleiben garantiert gleich
// 308 Permanent Redirect          - wie 301, Methode/Body bleiben garantiert gleich
// 400 Bad Request                 - Request fehlerhaft/nicht interpretierbar     [Parser]
// 403 Forbidden                   - Zugriff verweigert
// 404 Not Found                   - Ressource existiert nicht
// 405 Method Not Allowed          - HTTP-Methode fuer diese Route nicht erlaubt  [Parser]
// 408 Request Timeout             - Client hat zu lange nichts geschickt         [Socket]
// 411 Length Required             - Body ohne Content-Length geschickt
// 413 Payload Too Large           - Body groesser als clientMaxBodySize          [Parser]
// 414 URI Too Long                - Request-Line laenger als erlaubt             [Parser]
// 431 Request Header Fields Too Large - Header-Block zu gross                    [Parser]
// 500 Internal Server Error       - unerwarteter Serverfehler (z.B. Datei nicht schreibbar)
// 501 Not Implemented             - Feature nicht unterstuetzt (z.B. fremdes Transfer-Encoding) [Parser]
// 505 HTTP Version Not Supported  - alles ausser HTTP/1.1                        [Parser]
//
// [Parser] = wird von HttpParser::parse ueber Request::errorCode geliefert
// [Socket] = kommt spaeter aus dem Client-Timeout im Server-Loop

std::string getStatusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 411: return "Length Required";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown";
    }
}

Response makeErrorResponse(int code, const LocationConfig& loc) {
    Response res;
    res.statusCode = code;
    res.statusText = getStatusText(code);

    std::string errorPath = loc.root;
    if (!errorPath.empty() && errorPath.back() == '/')
        errorPath.pop_back();
    errorPath += "/error/" + std::to_string(code) + ".html";

    /*
    Gleiche Falle wie in serveFile: ein ifstream auf ein Verzeichnis meldet is_open() == true
    und liest 0 Bytes. Ohne den is_regular_file-Check kaeme eine Fehlerseite mit leerem Body
    und Content-Type: text/html raus, statt auf den Plaintext-Fallback zu fallen.
    */
    std::error_code ec;
    const bool usable = std::filesystem::is_regular_file(errorPath, ec);

    std::ifstream file(errorPath);
    if (usable && file.is_open()) {
        std::stringstream ss;
        ss << file.rdbuf();
        res.body = ss.str();
        res.headers["Content-Type"] = "text/html";
    } else {
        res.body = std::to_string(code) + " " + res.statusText;
        res.headers["Content-Type"] = "text/plain";
    }
    res.headers["Content-Length"] = std::to_string(res.body.size());
    return res;
}

