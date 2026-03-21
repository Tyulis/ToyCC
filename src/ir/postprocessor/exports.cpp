#include "ir/postprocessor.h"
#include "arch/datamodel.h"

namespace toycc::ir {
    std::shared_ptr<Scope> PostProcessor::process(std::shared_ptr<Scope> global_scope) {
        PostProcessor postprocessor(global_scope);
        return postprocessor();
    }

    PostProcessor::PostProcessor(std::shared_ptr<Scope> global_scope) : global_scope(global_scope), unique_id(0),
            offset_type(std::make_shared<IntegerType>(".PToffset", BUILTIN_LOCATION, arch::DATAMODEL->pointer_size(), arch::DATAMODEL->pointer_alignment(), false)){}

    std::shared_ptr<Scope> PostProcessor::operator() () {
        global_scope->clear_types();  // After semantic analysis, we won't need to resolve type names anymore
        split_blocks(global_scope);
        split_indirections(global_scope);
        dereference(global_scope);  // Must go after splitting indirections and block accesses
        descope(global_scope);
        return global_scope;
    }
}
