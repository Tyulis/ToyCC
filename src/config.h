#pragma once

namespace toycc::config {
    namespace debug {
        extern bool with_comment_trace;
        extern bool with_translation_trace;
    }

    namespace optimization {
        extern bool split_intermediates;
    }
}
