#pragma once

#include <string>

namespace webserv {

class Logger {
public:
    Logger();
    ~Logger();

    void info(const std::string &message) const;
    void warn(const std::string &message) const;
    void error(const std::string &message) const;
};

} // namespace webserv
