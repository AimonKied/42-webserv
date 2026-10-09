/*
multipart/form-data ist das Format, in dem ein Browser ein HTML-Formular mit Datei per POST
schickt. Der Body besteht aus mehreren Teilen, einer pro Formularfeld. Getrennt werden sie
durch eine boundary, die im Content-Type-Header steht:

    Content-Type: multipart/form-data; boundary=----abc

    ------abc
    Content-Disposition: form-data; name="datei"; filename="bild.jpg"
    Content-Type: image/jpeg

    <rohe Bytes der Datei>
    ------abc--

Jeder Teil hat eigene Header, dann eine Leerzeile, dann den Inhalt.

Ablauf: boundary aus dem Content-Type holen, dann den Body Teil fuer Teil durchgehen, bis ein
Teil mit filename= kommt. Zurueckgegeben werden der Dateiname (nur der letzte Teil, damit
"../" nicht aus dem Upload-Ordner herausfuehrt) und der Inhalt. Andere Felder werden
ignoriert, kaputtes Format ergibt 400.

Aufgerufen wird das in buildPost und nicht im Parser, weil CGI-Skripte den Body unveraendert
brauchen.
*/
#include "Multipart.hpp"
#include <cctype>

static std::string toLower(const std::string& str) {
    std::string lower = str;

    for (char& c : lower)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower;
}

static std::string trimSpaces(const std::string& str) {
    const size_t start = str.find_first_not_of(" \t");
    if (start == std::string::npos)
        return "";
    const size_t end = str.find_last_not_of(" \t");
    return str.substr(start, end - start + 1);
}

bool isMultipartFormData(const std::string& contentType) {
    const std::string mediaType = trimSpaces(contentType.substr(0, contentType.find(';')));

    return toLower(mediaType) == "multipart/form-data";
}

static std::string extractParameter(const std::string& headerValue, const std::string& name) {
    const std::string lower = toLower(headerValue);
    const std::string key = name + "=";

    size_t pos = lower.find(key);
    while (pos != std::string::npos && pos > 0
           && lower[pos - 1] != ';' && lower[pos - 1] != ' ' && lower[pos - 1] != '\t')
        pos = lower.find(key, pos + 1);
    if (pos == std::string::npos)
        return "";

    const size_t start = pos + key.size();
    if (start < headerValue.size() && headerValue[start] == '"') {
        const size_t end = headerValue.find('"', start + 1);
        if (end == std::string::npos)
            return "";
        return headerValue.substr(start + 1, end - start - 1);
    }
    const size_t end = headerValue.find(';', start);
    return trimSpaces(headerValue.substr(start, end - start));
}

static std::string extractBoundary(const std::string& contentType) {
    const std::string boundary = extractParameter(contentType, "boundary");

    if (boundary.size() > 70)
        return "";
    return boundary;
}

static std::string extractFilename(const std::string& partHeaders) {
    size_t lineStart = 0;

    while (lineStart < partHeaders.size()) {
        size_t lineEnd = partHeaders.find("\r\n", lineStart);
        if (lineEnd == std::string::npos)
            lineEnd = partHeaders.size();

        const std::string line = partHeaders.substr(lineStart, lineEnd - lineStart);
        const size_t colon = line.find(':');
        if (colon != std::string::npos
            && toLower(trimSpaces(line.substr(0, colon))) == "content-disposition")
            return extractParameter(line.substr(colon + 1), "filename");

        lineStart = lineEnd + 2;
    }
    return "";
}

static std::string sanitizeFilename(const std::string& filename) {
    const size_t lastSeparator = filename.find_last_of("/\\");
    const std::string name = (lastSeparator == std::string::npos)
        ? filename : filename.substr(lastSeparator + 1);

    if (name.empty() || name == "." || name == "..")
        return "";
    for (char c : name) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 0x20 || uc == 0x7f)
            return "";
    }
    return name;
}

static MultipartFile multipartError() {
    MultipartFile result;

    result.errorCode = 400;
    return result;
}

MultipartFile parseMultipart(const std::string& contentType, const std::string& body) {
    const std::string boundary = extractBoundary(contentType);
    if (boundary.empty())
        return multipartError();

    const std::string delimiter = "--" + boundary;
    const std::string nextDelimiter = "\r\n" + delimiter;

    size_t pos = body.find(delimiter);
    if (pos == std::string::npos)
        return multipartError();

    while (true) {
        pos += delimiter.size();
        if (body.compare(pos, 2, "--") == 0)
            return multipartError();
        if (body.compare(pos, 2, "\r\n") != 0)
            return multipartError();
        pos += 2;

        const size_t contentEnd = body.find(nextDelimiter, pos);
        const size_t headersEnd = body.find("\r\n\r\n", pos);
        if (contentEnd == std::string::npos || headersEnd == std::string::npos
            || headersEnd + 4 > contentEnd)
            return multipartError();
        const size_t contentStart = headersEnd + 4;

        const std::string filename = extractFilename(body.substr(pos, headersEnd - pos));
        if (!filename.empty()) {
            MultipartFile result;
            result.filename = sanitizeFilename(filename);
            if (result.filename.empty())
                return multipartError();
            result.content = body.substr(contentStart, contentEnd - contentStart);
            return result;
        }
        pos = contentEnd + 2;
    }
}
