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

static std::string toLower(const std::string& str) {
	std::string result;
	for (unsigned char c : str)
		result += static_cast<char>(std::tolower(c));
	return result;
}

/* CGI Response
	Schritt 1: Header und Body trennen. Header ist bis zur ersten leeren Zeile (\r\n\r\n oder \n\n), Body alles danach.
	Schritt 2: Header parsen. Name ist bis zum ersten Doppelpunkt, Value alles danach (ohne fuehrende Leerzeichen).
	Schritt 3: Status bestimmen. "Status"-Header gewinnt, sonst Location ohne Status -> 302 Found, sonst 200 OK.
	Schritt 4: Content-Length selbst aus der Body-Laenge setzen.

	502 Bad Gateway, wenn:
	- keine leere Zeile zwischen Header und Body
	- Header-Zeile ohne ':' oder mit leerem Namen
	- Status keine Zahl oder ausserhalb von 100-599
	- weder Status, Location noch Content-Type vorhanden

	Bsp. 1 (normaler Fall):
	"Content-Type: text/html\n\n<h1>Hallo</h1>"
	-> 200 OK, Content-Type: text/html, Content-Length: 14

	Bsp. 2 (Redirect):
	"Location: /login\n\n"
	-> 302 Found, Location: /login, Content-Length: 0

	Bsp. 3 (eigener Status, falsche Laenge vom Skript):
	"Status: 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 99\r\n\r\nWeg"
	-> 404 Not Found, Content-Type: text/plain, Content-Length: 3
*/

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

	bool hasStatus = false;
	bool hasLocation = false;
	bool hasContentType = false;

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

		std::string lowerName = toLower(name);
		if (lowerName == "status") {
			std::istringstream statusStream(value);
			int code;
			if (!(statusStream >> code) || code < 100 || code > 599)
				return makeErrorResponse(502, loc);

			std::string text;
			std::getline(statusStream, text);
			size_t textStart = text.find_first_not_of(" \t");
			res.statusCode = code;
			res.statusText = (textStart == std::string::npos) ? getStatusText(code) : text.substr(textStart);
			hasStatus = true;
			continue;
		}
		if (lowerName == "content-length")
			continue;
		if (lowerName == "location")
			hasLocation = true;
		if (lowerName == "content-type")
			hasContentType = true;

		res.headers[name] = value;
	}

	if (!hasStatus && !hasLocation && !hasContentType)
		return makeErrorResponse(502, loc);

	if (hasLocation && !hasStatus) {
		res.statusCode = 302;
		res.statusText = getStatusText(302);
	}

	res.headers["Content-Length"] = std::to_string(res.body.size());
	return res;
}