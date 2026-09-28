#include "ConfigParser.hpp"

#include <stdexcept>

ConfigParser::ConfigParser() {}
ConfigParser::~ConfigParser() {}

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
