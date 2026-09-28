#pragma once

#include "ConfigTypes.hpp"
#include <cstddef>
#include <string>
#include <vector>

class ConfigParser {
public:
    ConfigParser();
    ~ConfigParser();

    Config parseFile(const std::string &path) const;

private:
    int parsePort(const std::string& value) const;
    std::string take(const std::vector<std::string>& tokens, std::size_t& position) const;
    void expect(const std::vector<std::string>& tokens, std::size_t& position,
                const std::string& expected) const;
};
