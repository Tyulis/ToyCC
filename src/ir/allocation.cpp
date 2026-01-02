#include "ir/allocation.h"

namespace toycc::ir {
    // Get the position of the requested variable in the stack frame.
    // If the variable is not yet in the stack frame, add it
    size_t StackFrame::position(std::shared_ptr<Declaration> declaration) {
        size_t position = current_position;
        current_position += declaration->type->size(declaration->location);

        locals[declaration] = position;
        return position;
    }
}
