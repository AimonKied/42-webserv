#include "ConfigParser.hpp"
#include "ConfigTokenizer.hpp"

#include <stdexcept>

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
        while (position < tokens.size() && tokens[position] != "}") {
            const std::string directive = take(tokens, position);
            if (directive == "listen") {
                if (server.port != 0) {
                    throw std::runtime_error("Duplicate listen directive in server block");
                }
                server.port = parsePort(take(tokens, position));
                expect(tokens, position, ";");
            } else {
                throw std::runtime_error("Unknown server directive: " + directive);
            }
        }
        expect(tokens, position, "}");
        config.push_back(server);
    }

    if (config.empty()) {
        throw std::runtime_error("Configuration must contain at least one server block");
    }
    return config;
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
