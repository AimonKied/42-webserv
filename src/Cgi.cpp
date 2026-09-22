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

static std::string methodToString(Method method) {
    switch (method) {
        case Method::GET:    return "GET";
        case Method::POST:   return "POST";
        case Method::DELETE: return "DELETE";
        default:             return "";
    }
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

    return env;
}
