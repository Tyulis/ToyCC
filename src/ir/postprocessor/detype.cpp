#include "diagnostic.h"
#include "ir/type_expressions.h"
#include "ir/postprocessor.h"

namespace toycc::ir {
    // Reduce all types to their raw storage type
    void PostProcessor::detype(std::shared_ptr<Scope> scope) {
        for (std::shared_ptr<Declaration> decl : scope->locals_list())
            decl->type = to_storage_type(decl->type);

        for (std::shared_ptr<Statement> statement : scope->statements) {
            if (statement->tag == stmt::Tag::BLOCK || statement->tag == stmt::Tag::FUNCTION) {
                std::shared_ptr<stmt::Block> block = std::static_pointer_cast<stmt::Block>(statement);
                detype(block->scope);
            }
        }

        scope->clear_types();  // After that, we won't need to resolve any type names
    }

    std::shared_ptr<Type> PostProcessor::to_storage_type(std::shared_ptr<Type> type) {
        switch (type->category) {
            case TypeCategory::VOID:
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Found a void object in IR postprocessing", type->location);
            case TypeCategory::BUILTIN:
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Detyping of built-in types is not implemented", type->location);

            case TypeCategory::BOOL:
            case TypeCategory::INTEGER:
            case TypeCategory::FLOAT:
                return type;

            case TypeCategory::POINTER:
                return pointer_storage_type;  // All pointers and arrays have the same representation

            case TypeCategory::ARRAY:
                return to_array_storage_type(std::static_pointer_cast<ArrayType>(type));

            case TypeCategory::STRUCT:
            case TypeCategory::UNION:
                return to_compound_storage_type(std::static_pointer_cast<CompoundType>(type));

            case TypeCategory::ENUM:
                return to_storage_type(std::static_pointer_cast<EnumType>(type)->underlying_type);

            case TypeCategory::FUNCTION:
                return to_function_storage_type(std::static_pointer_cast<FunctionType>(type));

            case TypeCategory::BITFIELD:
                return to_bitfield_storage_type(std::static_pointer_cast<BitfieldType>(type));

            case TypeCategory::QUALIFIED:
                return to_qualified_storage_type(std::static_pointer_cast<QualifiedType>(type));

            case TypeCategory::ALIGNED:
                return to_aligned_storage_type(std::static_pointer_cast<AlignedType>(type));
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown type category", type->location);
    }

    std::shared_ptr<Type> PostProcessor::to_array_storage_type(std::shared_ptr<ArrayType> type) {
        return ArrayType::make(type->name, type->location, to_storage_type(type->element_type), type->length);
    }

    std::shared_ptr<Type> PostProcessor::to_compound_storage_type(std::shared_ptr<CompoundType> type) {
        if (!type->is_complete)
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Type `{}` was never completed", type->name), type->location);

        std::vector<Member> detyped_members;
        for (Member member : type->members)
            detyped_members.emplace_back(member.name, to_storage_type(member.type), member.location);

        switch (type->category) {
            case TypeCategory::STRUCT:  return StructType::make(type->name, type->location, true, detyped_members);
            case TypeCategory::UNION:   return UnionType::make (type->name, type->location, true, detyped_members);
            default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown compound type category", type->location);
        }
    }

    std::shared_ptr<Type> PostProcessor::to_function_storage_type(std::shared_ptr<FunctionType> type) {
        std::vector<Member> detyped_parameters;
        for (Member parameter : type->parameters)
            detyped_parameters.emplace_back(parameter.name, to_storage_type(parameter.type), parameter.location);

        std::shared_ptr<Type> return_type = type->return_type;
        if (return_type->category != TypeCategory::VOID)
            return_type = to_storage_type(return_type);
        return FunctionType::make(type->name, type->location, return_type, detyped_parameters);
    }

    std::shared_ptr<Type> PostProcessor::to_bitfield_storage_type(std::shared_ptr<BitfieldType> type) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Bitfields are currently not simplified by Bitfield::make", type->location);
        return BitfieldType::make(type->name, type->location, to_storage_type(type->underlying_type), type->size_bits);
    }

    std::shared_ptr<Type> PostProcessor::to_aligned_storage_type(std::shared_ptr<AlignedType> type) {
        return AlignedType::make(type->name, type->location, to_storage_type(type->underlying_type), type->alignment_bits);
    }

    std::shared_ptr<Type> PostProcessor::to_qualified_storage_type(std::shared_ptr<QualifiedType> type) {
        return QualifiedType::make(type->name, type->location, to_storage_type(type->underlying_type), type->qualifiers);
    }

}
