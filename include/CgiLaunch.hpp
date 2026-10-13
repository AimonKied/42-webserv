#pragma once

#include <string>
#include <vector>

namespace webserv {

// Routing supplies these values; the executor owns processes and descriptors.
struct CgiLaunch {
	std::string executable;
	std::string scriptPath;
	std::string workingDirectory;
	std::vector<std::string> environment;
	std::string input;
};

} // namespace webserv
