#include "ir/postprocessor.h"
#include "arch/datamodel.h"

namespace toycc::ir {
    PostProcessor::PostProcessor(std::shared_ptr<Scope> global_scope) : global_scope(global_scope), unique_id(0),
            offset_type(std::make_shared<IntegerType>(".PToffset", BUILTIN_LOCATION, arch::DATAMODEL->pointer_size(), arch::DATAMODEL->pointer_alignment(), false)){}

    TranslationUnit PostProcessor::operator() () {
        dereference(global_scope);  // Requires semantic pointer types -> must come before `detype`
        detype(global_scope);
        descope(global_scope);

        TranslationUnit unit(global_scope);

        return unit;
    }
}
