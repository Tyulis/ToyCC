#include "diagnostic.h"
#include "code_location.h"
#include "ir/compound_type.h"


namespace toycc::ir {
    CompoundType::CompoundType(TypeIdentifier identifier, CodeLocation location) : Type(identifier, location) {
        if (identifier.category != TypeCategory::STRUCT && identifier.category != TypeCategory::UNION)
            throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, "Invalid type category for a compound type", location);
    }
    Enum::Enum    (std::string name, CodeLocation location) : Type(TypeIdentifier {.category = TypeCategory::ENUM,   .name = name}, location) {}
}
