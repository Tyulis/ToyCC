#include "diagnostic.h"
#include "ir/type.h"
#include "semantic/analyzer.h"

namespace toycc::semantic {
    // -------- Initializers
    void SemanticAnalyzer::decode_initializer(CParser::InitializerContext* context, std::shared_ptr<Declaration> variable) {
        if (context->LeftBrace() || context->RightBrace()) {
            if (context->initializerList())
                emit_copy(variable, decode_initializer_list(context->initializerList(), variable->type, locate(context)), locate(context), true);
            else
                default_initialize(variable, locate(context));
        } else if (context->assignmentExpression()) {
            ExpressionResult initializer = decode_assignment_expression(context->assignmentExpression());
            emit_copy(variable, initializer.operand(), locate(context), true);
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

    Constant SemanticAnalyzer::decode_array_initializer_list (CParser::InitializerListContext*, std::shared_ptr<ArrayType>,  const CodeLocation& location) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Array initializer lists are not implemented", location);
    }

    Constant SemanticAnalyzer::decode_struct_initializer_list(CParser::InitializerListContext*, std::shared_ptr<StructType>, const CodeLocation& location) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Struct initializer lists are not implemented", location);
    }

    Constant SemanticAnalyzer::decode_union_initializer_list (CParser::InitializerListContext*, std::shared_ptr<UnionType>,  const CodeLocation& location) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Union initializer lists are not implemented", location);
    }


    // 6.7.10.11 : Default initialization, for empty initializers (int variable = {}) and static / thread-local storage variables
    void SemanticAnalyzer::default_initialize(std::shared_ptr<Declaration> variable, const CodeLocation& location) {
        emit_copy(variable, default_initializer(variable->type, location), location, true);
    }

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
