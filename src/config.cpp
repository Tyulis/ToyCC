#include "config.h"

namespace toycc::config {
    namespace debug {
        bool enable = false;
        bool with_comment_trace = false;
        bool with_translation_trace = false;
    }

    namespace optimization {
        bool split_intermediates = true;
    }
}
