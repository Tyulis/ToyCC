#include <sstream>

#include "diagnostic.h"
#include "code_location.h"
#include "ir/compound_type.h"


namespace toycc::ir {
    std::string StructMember::ir_code() const {
        return std::format("#member {} : {}", name, spec.ir_code());
    }

    CompoundType::CompoundType(TypeIdentifier identifier, CodeLocation location) : Type(identifier, location) {
        if (identifier.category != TypeCategory::STRUCT && identifier.category != TypeCategory::UNION)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid type category for a compound type", location);
    }

    std::string CompoundType::ir_code() const {
        std::stringstream code;
        code << "#type " << identifier.ir_code() << " : {\n";
        for (const StructMember& member : members)
            code << "    " << member.ir_code() << "\n";
        code << "};";
        return code.str();
    }

    Enum::Enum (std::string name, CodeLocation location) : Type(TypeIdentifier {.category = TypeCategory::ENUM, .name = name}, location) {}

    std::string Enum::ir_code() const {
        std::stringstream code;
        code << "#type " << identifier.ir_code() << " : {\n";
        for (std::pair<std::string, int> value : values)
            code << "    " << value.first << " = " << value.second << "\n";
        code << "};";
        return code.str();
    }
}
