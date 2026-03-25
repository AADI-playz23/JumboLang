#ifndef JUMBOLANG_FILESYSTEM_H
#define JUMBOLANG_FILESYSTEM_H

#include <string>

class FileSystem {
public:
    static void write(const std::string& path, const std::string& content);
    static std::string read(const std::string& path);
};

#endif