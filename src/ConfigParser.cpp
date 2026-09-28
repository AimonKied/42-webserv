#include "ConfigParser.hpp"
#include "ConfigTokenizer.hpp"

#include <stdexcept>
#include <algorithm>

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
        std::string defaultRoot;
        std::string defaultIndex;
        while (position < tokens.size() && tokens[position] != "}") {
            const std::string directive = take(tokens, position);
            if (directive == "listen") {
                if (server.port != 0) {
                    throw std::runtime_error("Duplicate listen directive in server block");
                }
                server.port = parsePort(take(tokens, position));
                expect(tokens, position, ";");
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
        if (directive != "root" && directive != "index") {
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
        } else {
            if (!location.index.empty()) {
                throw std::runtime_error("Duplicate index directive in location block");
            }
            location.index = value;
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
