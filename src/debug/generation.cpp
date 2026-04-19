#include <format>

#include "diagnostic.h"
#include "debug/debuginfo.h"
#include "debug/unit.h"
#include "ir/declaration.h"
#include "ir/flow.h"
#include "ir/type_expressions.h"

namespace toycc::debug {
    DebugInfoEntry CompilationUnit::procedure(const ir::Procedure& procedure) {
        std::shared_ptr<ir::Declaration> declaration = procedure.declaration;
        std::shared_ptr<ir::FunctionType> function_type = std::static_pointer_cast<ir::FunctionType>(declaration->type);

        DebugInfoEntry entry = DebugInfoEntry(Tag::DW_TAG_subprogram)
            .add(Attribute::DW_AT_name,            Form::DW_FORM_strp,  string(declaration->name))
            .add(Attribute::DW_AT_external,        Form::DW_FORM_flag,  !(declaration->storage & ir::StorageClass::STATIC))
            .add(Attribute::DW_AT_main_subprogram, Form::DW_FORM_flag,  declaration->name == "main")
            .add(Attribute::DW_AT_low_pc,          Form::DW_FORM_addr,  procedure.start_label())
            .add(Attribute::DW_AT_high_pc,         Form::DW_FORM_data8, std::format("{}-{}", procedure.end_label(), procedure.start_label()))
            .location(fileno(declaration->location.filename), declaration->location.line, declaration->location.character);

        // Return type : only if the function actually returns something
        if (function_type->return_type->category != ir::TypeCategory::VOID)
            entry.add(Attribute::DW_AT_type, Form::DW_FORM_ref8, type(function_type->return_type));

        return entry;
    }

    DebugInfoEntry CompilationUnit::variable(std::shared_ptr<ir::Declaration> declaration) {
        // Missing : DW_AT_declaration, DW_AT_location
        DebugInfoEntry entry = DebugInfoEntry(Tag::DW_TAG_variable)
            .add(Attribute::DW_AT_name,     Form::DW_FORM_strp, string(declaration->name))
            .add(Attribute::DW_AT_external, Form::DW_FORM_flag, (declaration->storage & ir::StorageClass::GLOBAL) && !(declaration->storage & ir::StorageClass::STATIC))
            .add(Attribute::DW_AT_type,     Form::DW_FORM_ref8, type(declaration->type))
            .location(fileno(declaration->location.filename), declaration->location.line, declaration->location.character);
        return entry;
    }


    // -------- Type entries
    void CompilationUnit::emit_type(std::shared_ptr<ir::Type> type_expression) {
        switch (type_expression->category) {
            case ir::TypeCategory::INTEGER:  return emit_integer_type(std::static_pointer_cast<ir::IntegerType>(type_expression));
            case ir::TypeCategory::POINTER:  return emit_pointer_type(std::static_pointer_cast<ir::PointerType>(type_expression));
            case ir::TypeCategory::ARRAY:    return emit_array_type  (std::static_pointer_cast<ir::ArrayType>  (type_expression));
            case ir::TypeCategory::STRUCT:   return emit_struct_type (std::static_pointer_cast<ir::StructType> (type_expression));

            case ir::TypeCategory::BOOL:
            case ir::TypeCategory::FLOAT:
            case ir::TypeCategory::UNION:
            case ir::TypeCategory::FUNCTION:
            case ir::TypeCategory::VOID:
            case ir::TypeCategory::BUILTIN:
            case ir::TypeCategory::LABEL:
            case ir::TypeCategory::ENUM:
            case ir::TypeCategory::BITFIELD:
            case ir::TypeCategory::QUALIFIED:
            case ir::TypeCategory::ALIGNED:
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Debug info emission is not implemented for type `{}`", type_expression->ir_code()));
        }
    }

    void CompilationUnit::emit_integer_type(std::shared_ptr<ir::IntegerType> type_expression) {
        append(DebugInfoEntry(Tag::DW_TAG_base_type)
            .add(Attribute::DW_AT_name,     Form::DW_FORM_strp,  string(type_expression->name))
            .add(Attribute::DW_AT_encoding, Form::DW_FORM_data1, (type_expression->is_signed ? BaseTypeEncoding::DW_ATE_signed : BaseTypeEncoding::DW_ATE_unsigned))
            .add(Attribute::DW_AT_bit_size, Form::DW_FORM_data1, type_expression->size_bits)
            .location(fileno(type_expression->location.filename), type_expression->location.line, type_expression->location.character));
    }

    void CompilationUnit::emit_pointer_type(std::shared_ptr<ir::PointerType> type_expression) {
        append(DebugInfoEntry(Tag::DW_TAG_pointer_type)
            .add(Attribute::DW_AT_type, Form::DW_FORM_ref8, type(type_expression->referenced_type))
            .location(fileno(type_expression->location.filename), type_expression->location.line, type_expression->location.character));
    }

    void CompilationUnit::emit_array_type(std::shared_ptr<ir::ArrayType> type_expression) {
        auto array_entry = push_auto(DebugInfoEntry(Tag::DW_TAG_array_type)
            .add(Attribute::DW_AT_type, Form::DW_FORM_ref8, type(type_expression->element_type))
            .location(fileno(type_expression->location.filename), type_expression->location.line, type_expression->location.character));

        DebugInfoEntry subrange_entry = DebugInfoEntry(Tag::DW_TAG_subrange_type)
            .add(Attribute::DW_AT_lower_bound, Form::DW_FORM_data1, 0);

        // Constant length -> set the upper bound ; otherwise, don't specify it so it's considered unknown (DWARF5 5.13)
        if (type_expression->length.is_constant() && type_expression->length.constant().tag() == ir::Constant::INTEGER)
            subrange_entry.add(Attribute::DW_AT_upper_bound, Form::DW_FORM_data8, static_cast<size_t>(type_expression->length.constant().integer()));

        append(subrange_entry);
    }

    void CompilationUnit::emit_struct_type(std::shared_ptr<ir::StructType> type_expression) {
        DebugInfoEntry struct_entry = DebugInfoEntry(Tag::DW_TAG_structure_type)
            .add(Attribute::DW_AT_byte_size, Form::DW_FORM_data8, type_expression->size(type_expression->location))
            .location(fileno(type_expression->location.filename), type_expression->location.line, type_expression->location.character);
        if (type_expression->name[0] != '.')  // Skip the generated name of anonymous structs
            struct_entry.add(Attribute::DW_AT_name, Form::DW_FORM_strp, string(type_expression->name));

        auto struct_scope = push_auto(struct_entry);
        for (const auto& [index, member] : std::ranges::enumerate_view(type_expression->members))
            emit_member(member, 8 * type_expression->member_offset(index));  // FIXME : Bitfields ?
    }

    void CompilationUnit::emit_member(const ir::Member& member, size_t bit_offset) {
        append(DebugInfoEntry(Tag::DW_TAG_member)
            .add(Attribute::DW_AT_name,            Form::DW_FORM_strp,  string(member.name))
            .add(Attribute::DW_AT_data_bit_offset, Form::DW_FORM_data4, bit_offset)
            .add(Attribute::DW_AT_type,            Form::DW_FORM_ref8,  type(member.type))
            .location(fileno(member.location.filename), member.location.line, member.location.character));
    }
}
