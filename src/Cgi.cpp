#include "Cgi.hpp"
#include <filesystem>
#include <cctype>
#include <sstream>

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

static std::string methodToString(Method method) {
    switch (method) {
        case Method::GET:    return "GET";
        case Method::POST:   return "POST";
        case Method::DELETE: return "DELETE";
        default:             return "";
    }
}

/* Der Parser speichert Header-Namen klein, fuer die Env muessen sie wieder gross. */
static std::string toEnvName(const std::string& headerName) {
    std::string result = "HTTP_";

    for (unsigned char c : headerName) {
        if (c == '-')
            result += '_';
        else
            result += static_cast<char>(std::toupper(c));
    }
    return result;
}

std::vector<std::string> buildCgiEnv(const Request& req, const CgiMatch& match, const ServerConfig& server, const std::string& scriptPath) {
    std::vector<std::string> env;

    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env.push_back("SERVER_SOFTWARE=webserv/1.0");
    env.push_back("SERVER_NAME=" + server.serverName);
    env.push_back("SERVER_PORT=" + std::to_string(server.port));

    env.push_back("REQUEST_METHOD=" + methodToString(req.method));
    env.push_back("REQUEST_URI=" + req.uri);
    env.push_back("SCRIPT_NAME=" + match.scriptName);
    env.push_back("SCRIPT_FILENAME=" + scriptPath);
    env.push_back("PATH_INFO=" + match.pathInfo);
    env.push_back("QUERY_STRING=" + req.query);
    env.push_back("REDIRECT_STATUS=200");
    env.push_back("CONTENT_LENGTH=" + std::to_string(req.body.size()));

    const auto contentType = req.headers.find("content-type");
    if (contentType != req.headers.end())
        env.push_back("CONTENT_TYPE=" + contentType->second);

    for (const auto& header : req.headers) {
        if (header.first == "content-type" || header.first == "content-length")
            continue;
        env.push_back(toEnvName(header.first) + "=" + header.second);
    }

    return env;
}

/* CGI Response */

Response buildCgiResponse(const std::string& output, const LocationConfig& loc) {
    Response res;

    size_t crlfPos = output.find("\r\n\r\n");
    size_t lfPos = output.find("\n\n");
    if (crlfPos == std::string::npos && lfPos == std::string::npos) {
        res = makeErrorResponse(502, loc);
        return res;
    }
    std::string headerPart;
    if (crlfPos < lfPos) {
        headerPart = output.substr(0, crlfPos);
        res.body = output.substr(crlfPos + 4);
    } else {
        headerPart = output.substr(0, lfPos);
        res.body = output.substr(lfPos + 2);
    }

    std::istringstream headerStream(headerPart);
    std::string line;
    while (std::getline(headerStream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;

        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos || colonPos == 0)
            return makeErrorResponse(502, loc);

        std::string name = line.substr(0, colonPos);
        size_t valueStart = line.find_first_not_of(" \t", colonPos + 1);
        std::string value;
        if (valueStart != std::string::npos)
            value = line.substr(valueStart);
        res.headers[name] = value;
    }

    return res;
}