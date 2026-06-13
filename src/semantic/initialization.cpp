#include "diagnostic.h"
#include "ir/type.h"
#include "semantic/analyzer.h"
#include "semantic/values.h"

namespace toycc::semantic {
    // -------- Initializers
    Operand SemanticAnalyzer::decode_initializer(CParser::InitializerContext* context, std::shared_ptr<Type> type) {
        if (context->LeftBrace() || context->RightBrace()) {
            if (context->initializerList())
                return decode_initializer_list(context->initializerList(), type);
            else
                return default_initializer(type, locate(context));
        } else if (context->assignmentExpression()) {
            ExpressionResult initializer = decode_assignment_expression(context->assignmentExpression());
            return initializer.operand();
        } else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown initializer `{}`", context->getText()), locate(context));
    }

    Constant SemanticAnalyzer::decode_initializer_list(CParser::InitializerListContext* context, std::shared_ptr<Type> type) {
        std::shared_ptr<Type> dequalified_type = type->dequalify();
        switch (dequalified_type->category) {
            case TypeCategory::ARRAY:
                return decode_array_initializer_list(context, std::static_pointer_cast<ArrayType>(dequalified_type));

            case TypeCategory::STRUCT:
                return decode_struct_initializer_list(context, std::static_pointer_cast<StructType>(dequalified_type));

            case TypeCategory::UNION:
                return decode_union_initializer_list(context, std::static_pointer_cast<UnionType>(dequalified_type));

            default: throw Diagnostic(DiagnosticLevel::ERROR, std::format("A variable of type {} can't have an initializer list", type->repr()), locate(context));
        }
        __builtin_unreachable();
    }

    Constant SemanticAnalyzer::decode_array_initializer_list (CParser::InitializerListContext* context, std::shared_ptr<ArrayType> type) {
        std::map<size_t, Operand> member_map;

        size_t current_index = 0;
        for (const auto& [initializer_index, initializer_context] : std::ranges::enumerate_view(context->designatedInitializer())) {
            const std::vector<Designation> designations = decode_designation(initializer_context->designation());
            const Operand value = decode_initializer(initializer_context->initializer(), type->element_type);
            for (const Designation& designation : designations) {
                switch (designation.tag()) {
                    case Designation::POSITIONAL:
                        member_map.insert({current_index++, value});
                        break;

                    case Designation::INDEX:
                        if (member_map.contains(designation.index()))
                            throw Diagnostic(DiagnosticLevel::ERROR, "Duplicate index in array initializer", locate(initializer_context));
                        member_map.insert({designation.index(), value});
                        current_index = designation.index() + 1;
                        break;

                    case Designation::NAME:
                        throw Diagnostic(DiagnosticLevel::ERROR, "Can't use name designators in array initializers", locate(initializer_context));
                }
            }
        }

        // std::map is in index order
        const Constant default_value = default_initializer(type->element_type, locate(context));
        std::vector<Constant> members;
        for (const auto& [index, value] : member_map) {
            while (members.size() < index)
                members.push_back(default_value);
            members.push_back(make_constant_initializer(value, type->element_type));
        }

        return Constant {members, locate(context), type};
    }

    Constant SemanticAnalyzer::decode_struct_initializer_list(CParser::InitializerListContext* context, std::shared_ptr<StructType> type) {
        std::map<size_t, Operand> member_map;

        size_t current_index = 0;
        for (const auto& [initializer_index, initializer_context] : std::ranges::enumerate_view(context->designatedInitializer())) {
            const std::vector<Designation> designations = decode_designation(initializer_context->designation());
            for (const Designation& designation : designations) {
                switch (designation.tag()) {
                    case Designation::POSITIONAL:
                        break;  // Use the `current_index`

                    case Designation::INDEX:
                        throw Diagnostic(DiagnosticLevel::ERROR, "Can't use index designators in array initializers", locate(initializer_context));

                    case Designation::NAME:
                        std::vector<size_t> position = type->member_index(designation.name());
                        if (position.size() > 1)
                            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Nested designators are not implemented", locate(initializer_context));
                        current_index = position[0];
                        break;
                }

                if (member_map.contains(current_index))
                    throw Diagnostic(DiagnosticLevel::ERROR, "Duplicate member in struct initializer", locate(initializer_context));

                const Operand value = decode_initializer(initializer_context->initializer(), type->members[current_index].type);
                member_map.insert({current_index++, value});
            }
        }

        // std::map is in index order
        std::vector<Constant> members;
        for (const auto& [index, value] : member_map) {
            const Constant default_value = default_initializer(type->members[index].type, locate(context));

            while (members.size() < index)
                members.push_back(default_value);

            members.push_back(make_constant_initializer(value, type->members[index].type));
        }

        return Constant {members, locate(context), type};
    }

    Constant SemanticAnalyzer::decode_union_initializer_list (CParser::InitializerListContext* context, std::shared_ptr<UnionType> type) {
        std::vector<CParser::DesignatedInitializerContext*> initializers = context->designatedInitializer();
        if (initializers.size() > 1)
            throw Diagnostic(DiagnosticLevel::ERROR, "Only one member of a union may be initialized at a time", locate(context));
        CParser::DesignatedInitializerContext* initializer_context = initializers[0];

        const std::vector<Designation> designations = decode_designation(initializer_context->designation());
        if (designations.size() > 1)
            throw Diagnostic(DiagnosticLevel::ERROR, "Only one member of a union may be initialized at a time", locate(context));
        const Designation& designation = designations[0];

        size_t member_index;
        switch (designation.tag()) {
            case Designation::POSITIONAL:
                member_index = 0;
                break;

            case Designation::INDEX:
                throw Diagnostic(DiagnosticLevel::ERROR, "Can't use index designators in union initializers", locate(initializer_context));

            case Designation::NAME:
                std::vector<size_t> position = type->member_index(designation.name());
                if (position.size() > 1)
                    throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Nested designators are not implemented", locate(initializer_context));
                member_index = position[0];
                break;
        }

        const Operand value = decode_initializer(initializer_context->initializer(), type->members[member_index].type);
        return make_constant_initializer(value, type->members[member_index].type);
    }

    Constant SemanticAnalyzer::make_constant_initializer(const Operand& value, std::shared_ptr<Type> type) {
        switch (value.tag()) {
            case Operand::CONSTANT:
                return value.constant();

            case Operand::VARIABLE:
                return {value.declaration(), value.location, type};

            case Operand::DEREFERENCE: {
                std::shared_ptr<Declaration> dereferenced = declare_temporary(type, value.location);
                emit_copy(dereferenced, value, value.location, true);
                return Constant {dereferenced, value.location, type};
            }
        }
        __builtin_unreachable();
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
        const std::vector<CParser::ConstantExpressionContext*> bounds = context->constantExpression();
        Constant begin = evaluate_constant_expression(bounds[0]);
        if (begin.tag() != Constant::INTEGER)
            throw Diagnostic(DiagnosticLevel::ERROR, "Array indices must be integers", locate(context));
        const size_t begin_index = static_cast<size_t>(begin.integer());

        if (bounds.size() == 1)
            return {{begin_index}};

        Constant end = evaluate_constant_expression(bounds[1]);
        if (end.tag() != Constant::INTEGER)
            throw Diagnostic(DiagnosticLevel::ERROR, "Array indices must be integers", locate(context));
        const size_t end_index = static_cast<size_t>(end.integer());

        std::vector<Designation> designations;
        for (size_t index = begin_index; index < end_index; index++)
            designations.emplace_back(index);
        return designations;
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
