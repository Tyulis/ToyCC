#pragma once

#include <string>
#include "debug/dwarf.h"

namespace toycc::config {
    namespace debug {
        extern bool enable;                       // Emit debug information in DWARF5 format
        extern bool with_default_location;        // Emit default locations for variables (not supported in current GDB releases)
        extern toycc::debug::DWARFFormat format;  // DWARF format to use (32 / 64-bits)
    }

    namespace optimization {
        extern bool split_intermediates;          // Split temporaries local to a building block as separate intermediate variables
    }

    namespace dev {
        extern bool with_comment_trace;           // Trace the code generation process in the generated assembly code
        extern bool with_location_trace;          // Add the variable movements to the comment trace
        extern bool with_translation_trace;       // Trace the code generation process on the standard output
    }

    std::string dump();
}
