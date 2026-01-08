#pragma once

#include <string>

#include "code_location.h"

namespace toycc::ir {
    enum class LabelType {
        NAMED, INTERNAL, FUNCTION,
    };

    struct Label {
        LabelType type;
        std::string name;
        CodeLocation location;
    };
}
