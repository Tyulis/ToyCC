#include "diagnostic.h"
#include "ir/type.h"
#include "semantic/analyzer.h"
#include "semantic/values.h"

namespace toycc::semantic {
    // -------- Initializers
    RValue SemanticAnalyzer::decode_initializer(CParser::InitializerContext* context, std::shared_ptr<Type> type) {
        if (context->LeftBrace() || context->RightBrace()) {
            if (context->initializerList())
                return decode_initializer_list(context->initializerList(), type, locate(context));
            else
                return default_initializer(type, locate(context));
        } else if (context->assignmentExpression()) {
            ExpressionResult initializer = decode_assignment_expression(context->assignmentExpression());
            return initializer.rvalue();
        } else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown initializer `{}`", context->getText()), locate(context));
    }

    Constant SemanticAnalyzer::decode_initializer_list(CParser::InitializerListContext* context, std::shared_ptr<Type> type, const CodeLocation& location) {
        std::shared_ptr<Type> dequalified_type = type->dequalify();
        switch (dequalified_type->category) {
            case TypeCategory::ARRAY:
                return decode_array_initializer_list(context, std::static_pointer_cast<ArrayType>(dequalified_type), location);

            case TypeCategory::STRUCT:
                return decode_struct_initializer_list(context, std::static_pointer_cast<StructType>(dequalified_type), location);

            case TypeCategory::UNION:
                return decode_union_initializer_list(context, std::static_pointer_cast<UnionType>(dequalified_type), location);

            default: throw Diagnostic(DiagnosticLevel::ERROR, std::format("A variable of type {} can't have an initializer list", type->repr()), location);
        }
        __builtin_unreachable();
    }

    Constant SemanticAnalyzer::decode_array_initializer_list (CParser::InitializerListContext* context, std::shared_ptr<ArrayType> type, const CodeLocation& location) {
        bool found_indexed_designation = false;
        std::map<size_t, RValue> member_map;
        for (const auto& [initializer_index, initializer_context] : std::ranges::enumerate_view(context->designatedInitializer())) {
            const std::vector<Designation> designations = decode_designation(initializer_context->designation());
            const RValue value = decode_initializer(initializer_context->initializer(), type->element_type);
            for (const Designation& designation : designations) {
                switch (designation.tag()) {
                    case Designation::POSITIONAL:
                        if (found_indexed_designation)
                            throw Diagnostic(DiagnosticLevel::ERROR, "Can't use positional initializers after an indexed initializer", locate(initializer_context));
                        member_map[initializer_index] = value;
                        break;

                    case Designation::INDEX:
                        if (member_map.contains(designation.index()))
                            throw Diagnostic(DiagnosticLevel::ERROR, "Duplicate index in array initializer", locate(initializer_context));
                        member_map[designation.index()] = value;
                        found_indexed_designation = true;
                        break;

                    case Designation::NAME:
                        throw Diagnostic(DiagnosticLevel::ERROR, "Can't use name designators in array initializers", locate(initializer_context));
                }
            }
        }

        // std::map is in index order
        const Constant default_value = default_initializer(type->element_type, location);
        std::vector<Constant> members;
        for (const auto& [index, value] : member_map) {
            while (members.size() < index)
                members.push_back(default_value);

            if (value.is_constant())
                members.push_back(value.constant());
            else
                members.push_back(Constant {value.declaration(), value.location(), type->element_type});
        }

        return Constant {members, location, type};
    }

    Constant SemanticAnalyzer::decode_struct_initializer_list(CParser::InitializerListContext*, std::shared_ptr<StructType>, const CodeLocation& location) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Struct initializer lists are not implemented", location);
    }

    Constant SemanticAnalyzer::decode_union_initializer_list (CParser::InitializerListContext*, std::shared_ptr<UnionType>,  const CodeLocation& location) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Union initializer lists are not implemented", location);
    }

    std::vector<Designation> SemanticAnalyzer::decode_designation(CParser::DesignationContext* context) {
        if (context == nullptr)
            return {{std::monostate {}}};

        if (context->designatorList()) {
            std::vector<Designation> designators;
            for (CParser::DesignatorContext* designator : context->designatorList()->designator())
                designators.append_range(decode_designator(designator));
            return designators;
        } else if (context->gnuArrayDesignator()) {
            return decode_gnu_array_designator(context->gnuArrayDesignator());
        } else if (context->gnuIdentifier()) {
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GNU identifier designations are not implemented", locate(context));
        } else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Invalid designation `{}`", context->getText()), locate(context));
    }

    std::vector<Designation> SemanticAnalyzer::decode_designator(CParser::DesignatorContext* context) {
        if (context->gnuArrayDesignator())
            return decode_gnu_array_designator(context->gnuArrayDesignator());
        else if (context->Identifier())
            return {{context->Identifier()->getText()}};
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Invalid designator `{}`", context->getText()), locate(context));
    }

    std::vector<Designation> SemanticAnalyzer::decode_gnu_array_designator(CParser::GnuArrayDesignatorContext* context) {
        Constant begin = evaluate_constant_expression
    }



    // 6.7.10.11 : Default initialization, for empty initializers (int variable = {}) and static / thread-local storage variables
    Constant SemanticAnalyzer::default_initializer(std::shared_ptr<Type> type, const CodeLocation& location) {
        std::shared_ptr<Type> dequalified_type = type->dequalify();
        switch (dequalified_type->category) {
            case TypeCategory::POINTER:  // Initialize with a null pointer
                return make_constant_zero(type, location).constant();

            case TypeCategory::FLOAT:
            case TypeCategory::INTEGER:
            case TypeCategory::BOOL:
            case TypeCategory::ENUM:  // Initialize with zero
                return make_constant_zero(type, location).constant();

            case TypeCategory::ARRAY:
                return array_default_initializer(std::static_pointer_cast<ArrayType>(dequalified_type), location);

            case TypeCategory::STRUCT:
                return struct_default_initializer(std::static_pointer_cast<StructType>(dequalified_type), location);

            case TypeCategory::UNION:
                return union_default_initializer(std::static_pointer_cast<UnionType>(dequalified_type), location);

            case TypeCategory::LABEL:
            case TypeCategory::FUNCTION:
            case TypeCategory::VOID:
            case TypeCategory::BUILTIN:
            case TypeCategory::BITFIELD:
            case TypeCategory::QUALIFIED:
            case TypeCategory::ALIGNED:
                throw Diagnostic(DiagnosticLevel::ERROR, std::format("Variable of type `{}` can't be default-initialized", type->repr()), location);
        }
        __builtin_unreachable();
    }

    Constant SemanticAnalyzer::array_default_initializer(std::shared_ptr<ArrayType> type, const CodeLocation& location) {
        switch (type->length.tag()) {
            case Operand::CONSTANT: {
                const std::vector<Constant> members = {default_initializer(type->element_type, location), Constant::make_repeat(location)};
                return Constant {members, location, type};
            }

            case Operand::VARIABLE:
            case Operand::DEREFERENCE:
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Default-initializing variable-length arrays is not implemented", location);
        }
        __builtin_unreachable();
    }

    Constant SemanticAnalyzer::struct_default_initializer(std::shared_ptr<StructType> type, const CodeLocation& location) {
        std::vector<Constant> members;
        for (const Member& member : type->members)
            members.push_back(default_initializer(member.type, location));
        return Constant {members, location, type};
    }

    Constant SemanticAnalyzer::union_default_initializer(std::shared_ptr<UnionType> type, const CodeLocation& location) {
        // 6.7.10.11 : To default-initialize a union, default-initialize its first member
        return Constant {UnionConstant {0, default_initializer(type->members[0].type, location)}, location, type};
    }
}
