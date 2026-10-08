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
        case 502: return "Bad Gateway";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown";
    }
}

std::string methodToString(Method method) {
    switch (method) {
        case Method::GET:     return "GET";
        case Method::POST:    return "POST";
        case Method::DELETE:  return "DELETE";
        case Method::UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
}

static std::string allowHeader(const std::vector<Method>& methods) {
    std::string result;

    for (Method method : methods) {
        if (!result.empty())
            result += ", ";
        result += methodToString(method);
    }
    return result;
}

/*
Eingebaute Fehlerseite fuer den Fall, dass keine eigene konfiguriert oder lesbar ist.
Das Subject verlangt das ausdruecklich ("Your server must have default error pages if none
are provided"), und eine Zeile text/plain sieht im Browser nach kaputtem Server aus.

Kein Escaping noetig: code ist ein int und statusText kommt aus getStatusText, also aus
einer festen Tabelle. Hier landet nichts, was aus einem Request stammt.
*/
static std::string defaultErrorPage(int code, const std::string& statusText) {
    const std::string number = std::to_string(code);

    return
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "<title>" + number + " " + statusText + "</title>\n"
        "<style>\n"
        "body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;"
        "font-family:system-ui,sans-serif;background:#fafafa;color:#111}\n"
        "main{text-align:center;padding:2rem}\n"
        "h1{margin:0;font-size:4rem;font-weight:600;letter-spacing:-.03em}\n"
        "p{margin:.5rem 0 0;font-size:1.125rem;color:#666}\n"
        "hr{margin:2rem auto 0;width:4rem;border:0;border-top:1px solid #ddd}\n"
        "small{display:block;margin-top:1rem;color:#999;font-size:.8125rem}\n"
        "@media(prefers-color-scheme:dark){body{background:#111;color:#eee}p{color:#999}"
        "hr{border-color:#333}small{color:#666}}\n"
        "</style>\n"
        "</head>\n"
        "<body>\n"
        "<main>\n"
        "<h1>" + number + "</h1>\n"
        "<p>" + statusText + "</p>\n"
        "<hr>\n"
        "<small>webserv</small>\n"
        "</main>\n"
        "</body>\n"
        "</html>\n";
}

/* Sucht nach errorPages in der LocationConfig. Falls gefunden, wird die passende Seite geladen. Falls nicht, returned false und Standarderrorpage wird verwendet */
static bool loadConfiguredErrorPage(int code, const LocationConfig& loc, std::string& body) {
    const auto entry = loc.errorPages.find(code);
    if (entry == loc.errorPages.end() || loc.root.empty())
        return false;

    const ResolvedPath target = resolvePath(entry->second, loc.root);
    std::error_code ec;
    if (target.errorCode != 0 || !std::filesystem::is_regular_file(target.path, ec))
        return false;

    std::ifstream file(target.path, std::ios::binary);
    if (!file.is_open())
        return false;
    std::stringstream ss;
    ss << file.rdbuf();
    body = ss.str();
    return true;
}

Response makeErrorResponse(int code, const LocationConfig& loc) {
    Response res;
    res.statusCode = code;
    res.statusText = getStatusText(code);

    if (!loadConfiguredErrorPage(code, loc, res.body))
        res.body = defaultErrorPage(code, res.statusText);
    if (code == 405)
        res.headers["Allow"] = allowHeader(loc.methods);
    res.headers["Content-Type"] = "text/html; charset=utf-8";
    res.headers["Content-Length"] = std::to_string(res.body.size());
    return res;
}

