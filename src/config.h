#pragma once

namespace toycc::config {
#ifdef WITH_COMMENT_TRACE
    constexpr bool with_comment_trace = true;
#else
    constexpr bool with_comment_trace = false;
#endif
}
