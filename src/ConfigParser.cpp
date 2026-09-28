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
        // Only empty blocks are supported until we add directive parsing.
        expect(tokens, position, "}");
        config.push_back(server);
    }

    if (config.empty()) {
        throw std::runtime_error("Configuration must contain at least one server block");
    }
    return config;
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
