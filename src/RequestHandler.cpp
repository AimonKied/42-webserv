#include "RequestHandler.hpp"
#include "Cgi.hpp"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace webserv {

RequestOutcome handleRequest(const Request& request, const ServerConfig& config) {
	const LocationConfig* matched = findBestLocation(request.path, config);
	if (!matched) {
		const int code = request.errorCode ? request.errorCode : 404;
		return {makeErrorResponse(code, LocationConfig()), nullptr};
	}
	const LocationConfig& location = *matched;
	// Reuse the HTTP layer's error, Allow-header and redirect construction.
	// These decisions precede CGI detection, so no script can bypass policy.
	if (request.errorCode || !request.complete || location.redirectCode ||
		std::find(location.methods.begin(), location.methods.end(), request.method) == location.methods.end())
		return {Response::build(request, location), matched};

	const CgiMatch match = resolveCgiTarget(request, location);
	if (!match.isCgi)
		return {Response::build(request, location), matched};
	// TEMPORARY interpreter policy: one supported CGI type, no shell invocation.
	if (location.cgiExtension != ".py") {
		return {makeErrorResponse(501, location), matched};
	}
	std::error_code error;
	const auto root = std::filesystem::canonical(location.root, error);
	if (error) {
		return {makeErrorResponse(500, location), matched};
	}
	const auto script = std::filesystem::weakly_canonical(root / std::filesystem::path(match.scriptName).relative_path(), error);
	if (error) {
		return {makeErrorResponse(403, location), matched};
	}
	auto rootPart = root.begin();
	auto scriptPart = script.begin();
	for (; rootPart != root.end() && scriptPart != script.end() && *rootPart == *scriptPart;
		 ++rootPart, ++scriptPart) {}
	if (rootPart != root.end()) {
		return {makeErrorResponse(403, location), matched};
	}
	if (!std::filesystem::is_regular_file(script, error)) {
		return {makeErrorResponse(404, location), matched};
	}
	CgiLaunch launch;
	launch.executable = "/usr/bin/python3";
	launch.scriptPath = script.string();
	launch.workingDirectory = script.parent_path().string();
	launch.environment = buildCgiEnv(request, match, config, launch.scriptPath);
	launch.input = request.body; // parser has already decoded chunked input
	return {std::move(launch), matched};
}

} // namespace webserv
