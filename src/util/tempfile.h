#pragma once

#include <string>
#include <filesystem>

namespace toycc {
    class TempFile {
        public:
            TempFile(std::filesystem::path basename);
            ~TempFile();
            operator std::filesystem::path() const;
            operator std::string() const;

        private:
            std::filesystem::path path;
    };
}
