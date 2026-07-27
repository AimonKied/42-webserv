#include "ConfigTokenizer.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

bool isWhitespace(char character) {
    return character == ' ' || character == '\t' ||
           character == '\n' || character == '\r';
}

bool isSymbol(char character) {
    return character == '{' || character == '}' || character == ';';
}

void addToken(std::vector<std::string> &tokens, std::string &current) {
    if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
    }
}

} // namespace

std::vector<std::string>
ConfigTokenizer::tokenizeFile(const std::string &path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open configuration file: " + path);
    }

    std::ostringstream content;
    content << file.rdbuf();
    if (file.bad()) {
        throw std::runtime_error("Could not read configuration file: " + path);
    }
    return tokenize(content.str());
}

std::vector<std::string>
ConfigTokenizer::tokenize(const std::string &content) const {
    std::vector<std::string> tokens;
    std::string current;
    bool insideQuotes = false;
    bool escaped = false;

    for (std::string::size_type index = 0; index < content.size(); ++index) {
        const char character = content[index];

        if (insideQuotes) {
            if (escaped) {
                current += character;
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                insideQuotes = false;
            } else {
                current += character;
            }
            continue;
        }

        if (character == '#') {
            addToken(tokens, current);
            while (index < content.size() && content[index] != '\n') {
                ++index;
            }
        } else if (character == '"') {
            insideQuotes = true;
        } else if (isWhitespace(character)) {
            addToken(tokens, current);
        } else if (isSymbol(character)) {
            addToken(tokens, current);
            tokens.push_back(std::string(1, character));
        } else {
            current += character;
        }
    }

    if (insideQuotes || escaped) {
        throw std::invalid_argument(
            "Unterminated quoted value in configuration file");
    }

    addToken(tokens, current);
    return tokens;
}
