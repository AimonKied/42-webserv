#include "HttpResponse.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>

// 200 OK                    - Request erfolgreich, Body enthaelt angeforderte Ressource
// 201 Created                - Ressource wurde neu angelegt (z.B. POST)
// 204 No Content             - Erfolgreich, aber kein Body (z.B. DELETE)
// 301 Moved Permanently      - Ressource dauerhaft umgezogen, Client darf cachen
// 302 Found                  - Ressource temporaer umgezogen, kein Caching
// 307 Temporary Redirect     - wie 302, Methode/Body bleiben garantiert gleich
// 308 Permanent Redirect     - wie 301, Methode/Body bleiben garantiert gleich
// 400 Bad Request            - Request fehlerhaft/nicht interpretierbar
// 403 Forbidden              - Zugriff verweigert
// 404 Not Found               - Ressource existiert nicht
// 405 Method Not Allowed      - HTTP-Methode fuer diese Route nicht erlaubt
// 500 Internal Server Error   - unerwarteter Serverfehler (z.B. Datei nicht schreibbar)

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
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

Response makeErrorResponse(int code, const std::string& rootDir) {
    Response res;
    res.statusCode = code;
    res.statusText = getStatusText(code);

    std::string errorPath = rootDir;
    if (!errorPath.empty() && errorPath.back() == '/')
        errorPath.pop_back();
    errorPath += "/error/" + std::to_string(code) + ".html";

    std::ifstream file(errorPath);
    if (file.is_open()) {
        std::stringstream ss;
        ss << file.rdbuf();
        res.body = ss.str();
        res.headers["Content-Type"] = "text/html";
    } else {
        res.body = std::to_string(code) + " " + res.statusText;
    }
    res.headers["Content-Length"] = std::to_string(res.body.size());
    return res;
}

