#pragma once

#include <string>

// Encode a decoded path for use in a URL, preserving directory separators.
inline std::string encodeUrlPath(const std::string& path) {
    static const char hex[] = "0123456789ABCDEF";
    std::string result;
    for (unsigned char c : path) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~' || c == '/') {
            result += static_cast<char>(c);
        } else {
            result += '%';
            result += hex[c >> 4];
            result += hex[c & 15];
        }
    }
    return result;
}
