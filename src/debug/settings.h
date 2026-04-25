#pragma once

namespace toycc::debug {
    constexpr char PRODUCER_IDENTIFICATION[] = "ToyCC";

    constexpr char BEGIN_TEXT_LABEL[] = ".LWL.text.begin";
    constexpr char END_TEXT_LABEL[]   = ".LWL.text.end";
    constexpr char BEGIN_DEBUG_ABBREV_LABEL[] = ".LWL.debug_abbrev.begin";
}
