#pragma once

#include "ConfigTypes.hpp"
#include <string>

class ConfigParser {
public:
    ConfigParser();
    ~ConfigParser();

    Config parseFile(const std::string &path) const;
};
