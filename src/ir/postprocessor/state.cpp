#include "ir/postprocessor.h"

namespace toycc::ir {
    std::string PostProcessor::anonymous_identifier() {
        return std::format(".PI{}", unique_id++);
    }

    std::string PostProcessor::anonymous_label() {
        return std::format(".PL{}", unique_id++);
    }

    std::string PostProcessor::anonymous_type() {
        return std::format(".PT{}", unique_id++);
    }

    std::string PostProcessor::make_scope_prefix(std::string name) {
        return std::format(".PS{}", name);
    }

    std::string PostProcessor::make_scope_prefix() {
        return std::format(".PS{}", unique_id++);
    }
}
