#include "ir/postprocessor/postprocessor.h"

namespace toycc::ir {
    std::string PostProcessor::anonymous_identifier() {
        return std::format(".PI{}", unique_id++);
    }

    std::string PostProcessor::anonymous_label() {
        return std::format(".LPL{}", unique_id++);
    }

    std::string PostProcessor::make_scope_prefix(std::string name) {
        return std::format(".PS{}", name);
    }

    std::string PostProcessor::make_scope_prefix() {
        return std::format(".PS{}", unique_id++);
    }

    std::shared_ptr<Declaration> PostProcessor::declare_temporary(std::shared_ptr<Scope> scope, std::shared_ptr<Type> type, CodeLocation location) {
        return scope->add_local(std::make_shared<Declaration>(anonymous_identifier(), type, location, StorageClass::AUTO | StorageClass::TEMPORARY));
    }

}
