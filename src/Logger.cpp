#include "Logger.hpp"
#include <iostream>

namespace webserv {

Logger::Logger() {
}

Logger::~Logger() {
}

void Logger::info(const std::string &message) const {
    std::cout << "[INFO] " << message << std::endl;
}

void Logger::warn(const std::string &message) const {
    std::cout << "[WARN] " << message << std::endl;
}

void Logger::error(const std::string &message) const {
    std::cerr << "[ERROR] " << message << std::endl;
}

} // namespace webserv
