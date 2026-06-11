// include/features/FileSystem.h
// Secure file I/O for JumboLang's {file} tag.
// All paths are validated before use to prevent path-traversal attacks.
#ifndef JUMBOLANG_FILESYSTEM_H
#define JUMBOLANG_FILESYSTEM_H

#include <string>

class FileSystem {
public:
    // Write content to a file (path must pass isSafePath validation)
    static void write(const std::string& path, const std::string& content);

    // Read and return file contents (path must pass isSafePath validation)
    static std::string read(const std::string& path);

    // Returns true only for relative, non-traversal paths.
    // Blocks: absolute paths, any path component equal to ".."
    static bool isSafePath(const std::string& path);
};

#endif // JUMBOLANG_FILESYSTEM_H