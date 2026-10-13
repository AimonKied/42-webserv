#pragma once

#include "CgiLaunch.hpp"
#include "HttpResponse.hpp"
#include <variant>

namespace webserv {

struct RequestOutcome {
	std::variant<Response, CgiLaunch> action;
	// Borrowed from Server's configuration, which is immutable during run().
	// Null only for a response with no matching location; CGI always has one.
	const LocationConfig* location;
};

RequestOutcome handleRequest(const Request& request, const ServerConfig& config);

} // namespace webserv
