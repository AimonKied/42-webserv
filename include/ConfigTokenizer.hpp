#pragma once

#include <string>
#include <vector>

class ConfigTokenizer {
public:
    std::vector<std::string> tokenizeFile(const std::string &path) const;
    std::vector<std::string> tokenize(const std::string &content) const;
};
