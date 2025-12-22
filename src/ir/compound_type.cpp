#include "code_location.h"
#include "ir/compound_type.h"


namespace toycc::ir {
    Struct::Struct(std::string name, CodeLocation location) : Type(TypeIdentifier {.category = TypeCategory::STRUCT, .name = name}, location) {}
    Union::Union  (std::string name, CodeLocation location) : Type(TypeIdentifier {.category = TypeCategory::UNION,  .name = name}, location) {}
    Enum::Enum    (std::string name, CodeLocation location) : Type(TypeIdentifier {.category = TypeCategory::ENUM,   .name = name}, location) {}
}
