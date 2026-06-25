#pragma once

#include <string>

namespace webserv {

class Utils {
public:
    Utils();
    ~Utils();

    static std::string trim(const std::string &value);
    static bool isWhitespace(char c);
};

} // namespace webserv
