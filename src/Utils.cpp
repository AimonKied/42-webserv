#include "Utils.hpp"

namespace webserv {

Utils::Utils() {
}

Utils::~Utils() {
}

std::string Utils::trim(const std::string &value) {
    // TODO: trim whitespace from both ends of the string
    return value;
}

bool Utils::isWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

} // namespace webserv
