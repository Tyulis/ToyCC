#pragma once

namespace toycc::config {
    namespace debug {
        extern bool enable;                  // Emit debug information in DWARF5 format
        extern bool with_comment_trace;      // Trace the code generation process in the generated assembly code
        extern bool with_translation_trace;  // Trace the code generation process on the standard output
    }

    namespace optimization {
        extern bool split_intermediates;
    }
}
