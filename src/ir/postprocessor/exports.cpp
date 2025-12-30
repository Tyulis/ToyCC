#include "arch/x86_64.h"
#include "ir/postprocessor.h"

namespace toycc::ir {
    PostProcessor::PostProcessor(std::shared_ptr<Scope> global_scope)
        : global_scope(global_scope), unique_id(0),
          pointer_storage_type(std::make_shared<IntegerType> ("@P.Tptr", BUILTIN_LOCATION, 8 * toycc::arch::POINTER_SIZE, 8 * toycc::arch::POINTER_ALIGNMENT, false))
          {}

    std::shared_ptr<Scope> PostProcessor::operator() () {
        detype(global_scope);
        descope(global_scope);
        return global_scope;
    }
}
