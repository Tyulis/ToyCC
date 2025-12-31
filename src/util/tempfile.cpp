#include <filesystem>
#include "util/tempfile.h"

namespace toycc {
    TempFile::TempFile(std::filesystem::path basename) : path(std::filesystem::temp_directory_path() / basename) {}
    TempFile::~TempFile() {
        if (std::filesystem::exists(path))
            std::filesystem::remove(path);
    }

    TempFile::operator std::filesystem::path() const {
        return path;
    }

    TempFile::operator std::string() const {
        return path;
    }
}
