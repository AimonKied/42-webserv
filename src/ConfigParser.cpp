#include "ConfigParser.hpp"
#include "ConfigTokenizer.hpp"

#include <stdexcept>
#include <algorithm>
#include <limits>

ConfigParser::ConfigParser() {}
ConfigParser::~ConfigParser() {}

Config ConfigParser::parseFile(const std::string& path) const {
    ConfigTokenizer tokenizer;
    const std::vector<std::string> tokens = tokenizer.tokenizeFile(path);
    std::size_t position = 0;
    Config config;

    while (position < tokens.size()) {
        expect(tokens, position, "server");
        expect(tokens, position, "{");

        ServerConfig server;
        server.host = "0.0.0.0";
        server.clientMaxBodySize = 1024 * 1024;
        bool bodySizeSeen = false;
        std::string defaultRoot;
        std::string defaultIndex;
        while (position < tokens.size() && tokens[position] != "}") {
            const std::string directive = take(tokens, position);
            if (directive == "listen") {
                if (server.port != 0) {
                    throw std::runtime_error("Duplicate listen directive in server block");
                }
                const std::string address = take(tokens, position);
                const std::size_t colon = address.find(':');
                if (colon == std::string::npos) {
                    server.port = parsePort(address);
                } else {
                    server.host = address.substr(0, colon);
                    validateIPv4(server.host);
                    server.port = parsePort(address.substr(colon + 1));
                }
                expect(tokens, position, ";");
            } else if (directive == "client_max_body_size") {
                if (bodySizeSeen) {
                    throw std::runtime_error("Duplicate client_max_body_size directive");
                }
                server.clientMaxBodySize = parseBodySize(take(tokens, position));
                expect(tokens, position, ";");
                bodySizeSeen = true;
            } else if (directive == "server_name") {
                if (!server.serverName.empty()) {
                    throw std::runtime_error("Duplicate server_name directive in server block");
                }
                const std::string name = take(tokens, position);
                if (name.empty() || name == ";" || name == "{" || name == "}" ||
                    name.find_first_of(" \t\r\n") != std::string::npos) {
                    throw std::runtime_error("server_name requires a single non-empty name");
                }
                expect(tokens, position, ";");
                server.serverName = name;
            } else if (directive == "error_page") {
                const std::string code = take(tokens, position);
                if (code.size() != 3 || (code[0] != '4' && code[0] != '5') ||
                    code[1] < '0' || code[1] > '9' || code[2] < '0' || code[2] > '9') {
                    throw std::runtime_error("error_page requires an error code between 400 and 599");
                }
                const int status = (code[0] - '0') * 100 + (code[1] - '0') * 10 + (code[2] - '0');
                if (server.errorPages.find(status) != server.errorPages.end()) {
                    throw std::runtime_error("Duplicate error_page for status " + code);
                }

                // Store a URL path to resolve under the configured root during serving.
                const std::string path = take(tokens, position);
                if (path.empty() || path[0] != '/' || path == "/") {
                    throw std::runtime_error("error_page requires a file path starting with '/'");
                }
                for (std::size_t i = 0; i < path.size(); ++i) {
                    const unsigned char character = static_cast<unsigned char>(path[i]);
                    if (character <= 0x20 || character == 0x7f) {
                        throw std::runtime_error("error_page path cannot contain whitespace or control characters");
                    }
                }
                expect(tokens, position, ";");
                server.errorPages[status] = path;
            } else if (directive == "root" || directive == "index") {
                const std::string value = take(tokens, position);
                if (value.empty() || value == ";" || value == "{" || value == "}") {
                    throw std::runtime_error(directive + " requires a non-empty path");
                }
                expect(tokens, position, ";");

                if (directive == "root") {
                    if (!defaultRoot.empty()) {
                        throw std::runtime_error("Duplicate root directive in server block");
                    }
                    defaultRoot = value;
                } else {
                    if (!defaultIndex.empty()) {
                        throw std::runtime_error("Duplicate index directive in server block");
                    }
                    defaultIndex = value;
                }
            } else if (directive == "location") {
                LocationConfig location = parseLocation(tokens, position);
                for (const LocationConfig& existing : server.locations) {
                    if (existing.path == location.path) {
                        throw std::runtime_error("Duplicate location path: " + location.path);
                    }
                }
                server.locations.push_back(location);
            } else {
                throw std::runtime_error("Unknown server directive: " + directive);
            }
        }
        expect(tokens, position, "}");

        // A valid listen directive always sets a non-zero port.
        if (server.port == 0) {
            throw std::runtime_error("Each server block requires a listen directive");
        }

        if (server.locations.empty()) {
            throw std::runtime_error("Each server block requires at least one location");
        }

        // Apply defaults after reading the whole server, regardless of directive order.
        if (defaultRoot.empty()) defaultRoot = "./www";
        if (defaultIndex.empty()) defaultIndex = "index.html";
        for (LocationConfig& location : server.locations) {
            if (location.root.empty()) location.root = defaultRoot;
            if (location.index.empty()) location.index = defaultIndex;
        }
        config.push_back(server);
    }

    if (config.empty()) {
        throw std::runtime_error("Configuration must contain at least one server block");
    }
    return config;
}

// Read a size in bytes. Zero allows only empty request bodies.
std::size_t ConfigParser::parseBodySize(const std::string& value) const {
    if (value.empty()) {
        throw std::runtime_error("client_max_body_size requires a number of bytes");
    }
    std::size_t size = 0;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] < '0' || value[i] > '9') {
            throw std::runtime_error("client_max_body_size must be a number of bytes");
        }
        const std::size_t digit = value[i] - '0';
        if (size > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
            throw std::runtime_error("client_max_body_size is too large");
        }
        size = size * 10 + digit;
    }
    return size;
}

// The caller has already consumed the "location" keyword.
LocationConfig ConfigParser::parseLocation(const std::vector<std::string>& tokens,
                                           std::size_t& position) const {
    LocationConfig location;
    bool autoindexSeen = false;
    location.path = take(tokens, position);
    if (location.path.empty() || location.path[0] != '/') {
        throw std::runtime_error("Location path must start with '/'");
    }

    expect(tokens, position, "{");
    while (position < tokens.size() && tokens[position] != "}") {
        const std::string directive = take(tokens, position);
        if (directive == "return") {
            if (location.redirectCode != 0) {
                throw std::runtime_error("Duplicate return directive in location block");
            }
            const std::string code = take(tokens, position);
            if (code == "301") location.redirectCode = 301;
            else if (code == "302") location.redirectCode = 302;
            else if (code == "307") location.redirectCode = 307;
            else if (code == "308") location.redirectCode = 308;
            else throw std::runtime_error("Redirect status must be 301, 302, 307, or 308");

            const std::string target = take(tokens, position);
            if (target.empty() || (target[0] != '/' &&
                target.compare(0, 7, "http://") != 0 &&
                target.compare(0, 8, "https://") != 0)) {
                throw std::runtime_error("Redirect destination must start with '/', 'http://', or 'https://'");
            }
            for (std::size_t i = 0; i < target.size(); ++i) {
                const unsigned char character = static_cast<unsigned char>(target[i]);
                if (character <= 0x20 || character == 0x7f) {
                    throw std::runtime_error("Redirect destination cannot contain whitespace or control characters");
                }
            }
            if (target == "http://" || target == "https://") {
                throw std::runtime_error("Redirect URL requires a destination after the scheme");
            }
            expect(tokens, position, ";");
            location.redirectTarget = target;
            continue;
        }
        if (directive == "allow_methods") {
            if (!location.methods.empty()) {
                throw std::runtime_error("Duplicate allow_methods directive in location block");
            }
            while (position < tokens.size() && tokens[position] != ";") {
                const std::string name = take(tokens, position);
                Method method;
                if (name == "GET") method = Method::GET;
                else if (name == "POST") method = Method::POST;
                else if (name == "DELETE") method = Method::DELETE;
                else throw std::runtime_error("Unsupported HTTP method: " + name);

                if (std::find(location.methods.begin(), location.methods.end(), method)
                    != location.methods.end()) {
                    throw std::runtime_error("Duplicate HTTP method: " + name);
                }
                location.methods.push_back(method);
            }
            if (location.methods.empty()) {
                throw std::runtime_error("allow_methods requires at least one method");
            }
            expect(tokens, position, ";");
            continue;
        }
        if (directive == "autoindex") {
            if (autoindexSeen) {
                throw std::runtime_error("Duplicate autoindex directive in location block");
            }
            const std::string value = take(tokens, position);
            if (value != "on" && value != "off") {
                throw std::runtime_error("autoindex requires 'on' or 'off'");
            }
            expect(tokens, position, ";");
            location.autoindex = (value == "on");
            autoindexSeen = true;
            continue;
        }
        if (directive == "cgi_extension") {
            if (!location.cgiExtension.empty()) {
                throw std::runtime_error("Duplicate cgi_extension directive in location block");
            }
            const std::string extension = take(tokens, position);
            if (extension.size() < 2 || extension[0] != '.') {
                throw std::runtime_error("cgi_extension requires an extension such as '.py'");
            }
            for (std::size_t i = 1; i < extension.size(); ++i) {
                const char character = extension[i];
                if (!((character >= 'a' && character <= 'z') ||
                      (character >= 'A' && character <= 'Z') ||
                      (character >= '0' && character <= '9'))) {
                    throw std::runtime_error("cgi_extension must contain only letters or digits after the dot");
                }
            }
            expect(tokens, position, ";");
            location.cgiExtension = extension;
            continue;
        }
        if (directive != "root" && directive != "index" && directive != "upload_store") {
            throw std::runtime_error("Unknown location directive: " + directive);
        }

        const std::string value = take(tokens, position);
        if (value.empty() || value == ";" || value == "{" || value == "}") {
            throw std::runtime_error(directive + " requires a non-empty path");
        }
        expect(tokens, position, ";");

        if (directive == "root") {
            if (!location.root.empty()) {
                throw std::runtime_error("Duplicate root directive in location block");
            }
            location.root = value;
        } else if (directive == "index") {
            if (!location.index.empty()) {
                throw std::runtime_error("Duplicate index directive in location block");
            }
            location.index = value;
        } else {
            if (!location.uploadStore.empty()) {
                throw std::runtime_error("Duplicate upload_store directive in location block");
            }
            location.uploadStore = value;
        }
    }
    expect(tokens, position, "}");
    // Preserve the existing server's methods when the directive is omitted.
    if (location.methods.empty()) {
        location.methods.push_back(Method::GET);
        location.methods.push_back(Method::POST);
        location.methods.push_back(Method::DELETE);
    }
    return location;
}

// Require four decimal numbers from 0 to 255, separated by dots.
void ConfigParser::validateIPv4(const std::string& address) const {
    std::size_t start = 0;
    for (int part = 0; part < 4; ++part) {
        const std::size_t dot = address.find('.', start);
        const std::string number = address.substr(start, dot == std::string::npos
                                                        ? std::string::npos : dot - start);
        if (number.empty() || number.size() > 3 ||
            (number.size() > 1 && number[0] == '0')) {
            throw std::runtime_error("Invalid listen IPv4 address: " + address);
        }
        int value = 0;
        for (std::size_t i = 0; i < number.size(); ++i) {
            if (number[i] < '0' || number[i] > '9') {
                throw std::runtime_error("Invalid listen IPv4 address: " + address);
            }
            value = value * 10 + (number[i] - '0');
        }
        if (value > 255 || (part < 3 && dot == std::string::npos) ||
            (part == 3 && dot != std::string::npos)) {
            throw std::runtime_error("Invalid listen IPv4 address: " + address);
        }
        if (part < 3) start = dot + 1;
    }
}

// Convert decimal digits to a port, rejecting invalid text and out-of-range values.
int ConfigParser::parsePort(const std::string& value) const {
    if (value.empty()) {
        throw std::runtime_error("Listen port cannot be empty");
    }

    int port = 0;
    for (char digit : value) {
        if (digit < '0' || digit > '9') {
            throw std::runtime_error("Invalid listen port: " + value);
        }
        port = port * 10 + (digit - '0');
        // Check each step so even a very long number cannot overflow.
        if (port > 65535) {
            throw std::runtime_error("Listen port must be between 1 and 65535");
        }
    }
    if (port == 0) {
        throw std::runtime_error("Listen port must be between 1 and 65535");
    }
    return port;
}

// Read one token and advance to the next one.
std::string ConfigParser::take(const std::vector<std::string>& tokens,
                               std::size_t& position) const {
    if (position >= tokens.size()) {
        throw std::runtime_error("Unexpected end of configuration");
    }
    return tokens[position++];
}

// Require a specific token, such as an opening brace or a semicolon.
void ConfigParser::expect(const std::vector<std::string>& tokens,
                          std::size_t& position, const std::string& expected) const {
    const std::string actual = take(tokens, position);
    if (actual != expected) {
        throw std::runtime_error("Config token " + std::to_string(position) +
                                 ": expected '" + expected + "', got '" + actual + "'");
    }
}
