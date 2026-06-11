// src/features/FileSystem.cpp
// Secure file I/O with path-traversal prevention.
// Uses std::filesystem (C++17) to inspect every path component before opening.
#include "../../include/features/FileSystem.h"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
// PATH SAFETY VALIDATOR
// Rejects:
//   • Absolute paths  (e.g. /etc/passwd, C:\Windows\System32\...)
//   • Any component equal to ".." (path-traversal attempt)
// ─────────────────────────────────────────────────────────────────────────────
bool FileSystem::isSafePath(const std::string& path) {
    if (path.empty()) {
        std::cerr << "    🔒 [SECURITY] Empty file path rejected.\n";
        return false;
    }
    try {
        fs::path p(path);

        // Block absolute paths (works cross-platform)
        if (p.is_absolute()) {
            std::cerr << "    🔒 [SECURITY] Blocked absolute path access: " << path << "\n";
            return false;
        }

        // Walk every component and reject ".."
        for (const auto& part : p) {
            if (part.string() == "..") {
                std::cerr << "    🔒 [SECURITY] Blocked path traversal attempt: " << path << "\n";
                return false;
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "    🔒 [SECURITY] Path validation error: " << e.what() << "\n";
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// WRITE
// ─────────────────────────────────────────────────────────────────────────────
void FileSystem::write(const std::string& path, const std::string& content) {
    if (!isSafePath(path)) {
        std::cerr << "    ❌ [FILE ERROR] Unsafe path rejected: " << path << "\n";
        return;
    }

    std::ofstream file(path);
    if (file.is_open()) {
        file << content;
        file.close();
        std::cout << "    💾 [FILE SYSTEM] Written " << content.size()
                  << " bytes to " << path << "\n";
    } else {
        std::cerr << "    ❌ [FILE ERROR] Could not open for writing: " << path << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// READ
// ─────────────────────────────────────────────────────────────────────────────
std::string FileSystem::read(const std::string& path) {
    if (!isSafePath(path)) {
        return "ERROR: Unsafe file path rejected.";
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "    ❌ [FILE ERROR] File not found: " << path << "\n";
        return "ERROR: File not found — " + path;
    }

    std::string content(
        (std::istreambuf_iterator<char>(file)),
         std::istreambuf_iterator<char>()
    );
    return content;
}