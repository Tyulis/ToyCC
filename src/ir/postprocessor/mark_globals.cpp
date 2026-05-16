#include "ir/postprocessor/postprocessor.h"

namespace toycc::ir {
    void PostProcessor::mark_globals(std::shared_ptr<Scope> global_scope) {
        // After descoping, only procedures and static declarations remain
        for (std::shared_ptr<Declaration> declaration : global_scope->locals_list())
            declaration->storage = StorageClass::GLOBAL;
    }
}
