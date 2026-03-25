#include "../../include/features/FileSystem.h"
#include <fstream>
#include <iostream>

void FileSystem::write(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (file.is_open()) {
        file << content;
        file.close();
        std::cout << "💾 [FILE SYSTEM] Data written to " << path << "\n";
    } else {
        std::cerr << "❌ [FILE ERROR] Could not write to " << path << "\n";
    }
}

std::string FileSystem::read(const std::string& path) {
    std::ifstream file(path);
    if (file.is_open()) {
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return content;
    }
    return "ERROR: File not found.";
}