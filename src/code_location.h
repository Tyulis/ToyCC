#pragma once

#include <string>

namespace toycc {
    struct CodeLocation {
        std::string filename;
        size_t line;
        size_t character;
    };
}
