#include "Server.hpp"
#include "Cgi.hpp"

#include <algorithm>
#include <filesystem>

namespace webserv {

void Server::queueResponse(Client& client, const Response& response) {
    client.writeBuffer = response.toString();
    client.readBuffer.clear();
    client.state = ClientState::Writing;
    client.lastActivity = std::time(nullptr);
    for (pollfd& entry : _fds)
        if (entry.fd == client.fd) entry.events = POLLOUT;
}

bool Server::dispatchCgi(Client& client, const Request& request, const ServerConfig& config) {
    // TEMPORARY routing handover: startup requires exactly one '/' location.
    // Simon's future response-or-CGI decision should replace this adapter.
    const LocationConfig& location = config.locations[0];
    const CgiMatch match = resolveCgiTarget(request, location);
    if (!match.isCgi) return false;
    if (std::find(location.methods.begin(), location.methods.end(), request.method) == location.methods.end()) {
        queueResponse(client, makeErrorResponse(405, location));
        return true;
    }
    if (location.redirectCode) {
        queueResponse(client, Response::buildRedirect(location.redirectCode, location.redirectTarget));
        return true;
    }
    // TEMPORARY interpreter policy: one supported CGI type, no shell invocation.
    if (location.cgiExtension != ".py") {
        queueResponse(client, makeErrorResponse(501, location));
        return true;
    }
    std::error_code error;
    const auto root = std::filesystem::canonical(location.root, error);
    if (error) {
        queueResponse(client, makeErrorResponse(500, location));
        return true;
    }
    const auto script = std::filesystem::weakly_canonical(root / std::filesystem::path(match.scriptName).relative_path(), error);
    if (error) {
        queueResponse(client, makeErrorResponse(403, location));
        return true;
    }
    auto rootPart = root.begin();
    auto scriptPart = script.begin();
    for (; rootPart != root.end() && scriptPart != script.end() && *rootPart == *scriptPart;
         ++rootPart, ++scriptPart) {}
    if (rootPart != root.end()) {
        queueResponse(client, makeErrorResponse(403, location));
        return true;
    }
    if (!std::filesystem::is_regular_file(script, error)) {
        queueResponse(client, makeErrorResponse(404, location));
        return true;
    }
    CgiLaunch launch;
    launch.executable = "/usr/bin/python3";
    launch.scriptPath = script.string();
    launch.workingDirectory = script.parent_path().string();
    launch.environment = buildCgiEnv(request, match, config, launch.scriptPath);
    launch.input = request.body; // parser has already decoded chunked input
    if (!_cgi.start(client.fd, launch)) {
        queueResponse(client, makeErrorResponse(502, location));
        return true;
    }
    client.readBuffer.clear();
    client.state = ClientState::WaitingForCgi;
    // Keep POLLIN for peer EOF. No socket response is ready to write yet.
    return true;
}

void Server::collectCgiResults() {
    for (CgiResult& result : _cgi.collectFinished()) {
        auto client = _clients.find(result.clientFd);
        if (client == _clients.end() || client->second.state != ClientState::WaitingForCgi)
            continue;
        const LocationConfig& location = _config.at(_listeners.at(client->second.listenerFd)).locations[0];
        queueResponse(client->second, result.errorCode
            ? makeErrorResponse(result.errorCode, location)
            : buildCgiResponse(result.output, location));
    }
}
} // namespace webserv
