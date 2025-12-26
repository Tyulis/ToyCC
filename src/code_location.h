#pragma once

#include <string>

namespace toycc {
    struct CodeLocation {
        std::string filename;
        size_t line = 0;
        size_t character = 0;
    };
}
