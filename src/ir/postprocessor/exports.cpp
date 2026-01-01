#include "ir/postprocessor.h"

namespace toycc::ir {
    PostProcessor::PostProcessor(std::shared_ptr<Scope> global_scope) : global_scope(global_scope), unique_id(0) {}

    TranslationUnit PostProcessor::operator() () {
        detype(global_scope);
        descope(global_scope);

        TranslationUnit unit = analyse_flow(global_scope);

        return unit;
    }
}
