#include "HttpResponse.hpp"
#include <filesystem>
#include <cctype>

Response makeErrorResponse(int code, const LocationConfig& loc);

/*
HTML-Escaping: Sonderzeichen in HTML-Entities umwandeln, damit sie nicht als HTML interpretiert werden.
&amp; -> &
&lt; -> <
&gt; -> >
&quot; -> "
&#39; -> '
*/
static std::string escapeHtml(const std::string& str) {
    std::string escaped;
    for (char c : str) {
        switch (c) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += c; break;
        }
    }
    return escaped;
}

/*
URL-Encoding: Zeichen ausserhalb der "unreserved"-Menge werden durch
%HH (Hex-Wert des Bytes) ersetzt, damit sie im href keine Sonderbedeutung haben
Leerzeichen, ?, #, &, %, ...
*/
static std::string urlEncode(const std::string& str) {
    static const char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (unsigned char c : str) {
        bool unreserved = std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
        if (unreserved)
            encoded += c;
        else {
            encoded += '%';
            encoded += hex[(c >> 4) & 0xF];
            encoded += hex[c & 0xF];
        }
    }
    return encoded;
}

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
        std::string name = entry.path().filename().string();
        bool isDir = entry.is_directory(ec);
        if (ec)
            continue;
        if (isDir) {
            res.body += "<li><a href=\"" + urlPath + urlEncode(name) + "/\">" + escapeHtml(name) + "/</a></li>";
        } else {
            res.body += "<li><a href=\"" + urlPath + urlEncode(name) + "\">" + escapeHtml(name) + "</a></li>";
        }
    }
    res.body += "</ul></body></html>";
    res.headers["Content-Type"] = "text/html";
    res.headers["Content-Length"] = std::to_string(res.body.size());
    return res;
}
