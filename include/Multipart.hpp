#pragma once

#include <string>

struct MultipartFile {
    std::string filename;
    std::string content;
    int errorCode = 0;      // 0 = ok
};

bool isMultipartFormData(const std::string& contentType);
MultipartFile parseMultipart(const std::string& contentType, const std::string& body);
