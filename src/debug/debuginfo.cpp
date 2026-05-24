#include "config.h"
#include "debug/debuginfo.h"
#include "debug/dwarf.h"
#include "debug/entries.h"
#include "debug/settings.h"
#include "ir/type.h"
#include "ir/type_expressions.h"

namespace toycc::debug {
    DebugInfo::DebugInfo(std::string working_directory, std::string filename, DWARFFormat format) : format(format), data(format) {
        const std::string length_expr = std::format("{}-{}", END_TEXT_LABEL, BEGIN_TEXT_LABEL);
        push(std::make_shared<CompilationUnitEntry>(BEGIN_TEXT_LABEL, length_expr, filename, working_directory));
    }

    // Manager and generator for .debug_info entries
    DebugInfo::EntryLifespan::~EntryLifespan() {
        debuginfo.pop();
    }


    // -------- Stack management
    void DebugInfo::push(std::shared_ptr<DebugInfoEntry> entry) {
        if (!stack.empty())
            stack.back()->children.push_back(entry);
        stack.push_back(entry);
    }

    void DebugInfo::append(std::shared_ptr<DebugInfoEntry> entry) {
        if (stack.empty())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't append a debug info entry with no parent");
        stack.back()->children.push_back(entry);
    }

    void DebugInfo::pop() {
        stack.pop_back();
    }

    DebugInfo::EntryLifespan DebugInfo::push_auto(std::shared_ptr<DebugInfoEntry> entry) {
        push(entry);
        return {*this};
    }


    // -------- Referenced entry management
    std::shared_ptr<TypeEntry> DebugInfo::type(std::shared_ptr<ir::Type> type) {
        auto it = types.find(type);
        if (it != types.end())
            return it->second;

        std::shared_ptr<TypeEntry> entry = add_type_entry(type);

        if (stack.empty())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't append a debug info entry with no parent");
        stack.front()->children.push_back(entry);  // Add types to the global scope
        return entry;
    }

    std::shared_ptr<VariableEntry> DebugInfo::variable(std::shared_ptr<ir::Declaration> declaration) {
        auto it = variables.find(declaration);
        if (it != variables.end())
            return it->second;

        // Missing : DW_AT_declaration, DW_AT_location
        const bool exported = (declaration->storage & ir::StorageClass::GLOBAL) && !(declaration->storage & ir::StorageClass::STATIC);
        std::shared_ptr<VariableEntry> entry = std::make_shared<VariableEntry> (declaration->name, exported, type(declaration->type), declaration->location);
        variables[declaration] = entry;
        return entry;
    }

    std::shared_ptr<SubprogramEntry> DebugInfo::procedure(const flow::Procedure& procedure) {
        std::shared_ptr<ir::Declaration> declaration = procedure.declaration;
        std::shared_ptr<ir::FunctionType> function_type = std::static_pointer_cast<ir::FunctionType>(declaration->type);

        Expression frame_base(format);
        frame_base.call_frame_cfa();

        // Return type : only if the function actually returns something
        std::shared_ptr<TypeEntry> return_type = nullptr;
        if (function_type->return_type->category != ir::TypeCategory::VOID)
            return_type = type(function_type->return_type);

        const bool exported = !(declaration->storage & ir::StorageClass::STATIC);
        const std::string length_expr = std::format("{}-{}", procedure.end_label(), procedure.start_label());
        return std::make_shared<SubprogramEntry> (declaration->name, exported, procedure.start_label(), length_expr, frame_base, return_type, declaration->location);
    }


    // -------- Actual code emission
    void DebugInfo::begin_text(CodeOutput& assembly) const {
        if (config::debug::enable)
            assembly.label(BEGIN_TEXT_LABEL);  // Emit the label that signals the beginning of the .text section, used in debug info
    }

    void DebugInfo::end_text(CodeOutput& assembly) const {
        if (config::debug::enable)
            assembly.label(END_TEXT_LABEL);
    }

    void DebugInfo::wrap_text(CodeOutput& assembly, const std::string& text_section) {
        if (!config::debug::enable) {
            assembly << text_section;
            return;
        }
        data.encode(stack.front());     // Recursively encode the root entry
        data.abbreviations.uleb128(0);  // The .debug_abbrev section must be null-terminated


        // Emit the file number directive for the line number information (.loc)
        data.filenos.emit(assembly);

        assembly << text_section;

        // Fill the .debug_info section
        assembly.directive(".section .debug_info,\"\",@progbits");
        assembly << data.debuginfo;

        // Fill the .debug_abbrev section
        assembly.directive(".section .debug_abbrev,\"\",@progbits");
        assembly.label(BEGIN_DEBUG_ABBREV_LABEL);
        assembly << data.abbreviations;

        // Fill the .debug_loclists section
        assembly.directive(".section .debug_loclists,\"\",@progbits");
        data.loclists.emit(assembly);

        // Fill the .debug_str section
        assembly.directive(".section .debug_str,\"\",@progbits");
        data.strings.emit(assembly);
    }


    // -------- Helpers
    size_t DebugInfo::fileno(const std::string& filename) {
        return data.filenos[filename];
    }

    Expression DebugInfo::expr() const {
        return Expression(format);
    }


    // -------- Type entry generation
    // Create a type entry and its children, add them to the `types` map, and return the type entry
    // NOTE : Entries are added into the map directly because it's necessary to guard against infinite recursion around recursive type expressions
    std::shared_ptr<TypeEntry> DebugInfo::add_type_entry(std::shared_ptr<ir::Type> type_expression) {
        switch (type_expression->category) {
            case ir::TypeCategory::BOOL:      return add_boolean_type_entry  (std::static_pointer_cast<ir::BooleanType>   (type_expression));
            case ir::TypeCategory::INTEGER:   return add_integer_type_entry  (std::static_pointer_cast<ir::IntegerType>   (type_expression));
            case ir::TypeCategory::POINTER:   return add_pointer_type_entry  (std::static_pointer_cast<ir::PointerType>   (type_expression));
            case ir::TypeCategory::ARRAY:     return add_array_type_entry    (std::static_pointer_cast<ir::ArrayType>     (type_expression));
            case ir::TypeCategory::STRUCT:    return add_struct_type_entry   (std::static_pointer_cast<ir::StructType>    (type_expression));
            case ir::TypeCategory::UNION:     return add_union_type_entry    (std::static_pointer_cast<ir::UnionType>     (type_expression));
            case ir::TypeCategory::QUALIFIED: return add_qualified_type_entry(std::static_pointer_cast<ir::QualifiedType> (type_expression));

            case ir::TypeCategory::VOID:     throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't generate debug info for `void`", type_expression->location);

            case ir::TypeCategory::FLOAT:
            case ir::TypeCategory::FUNCTION:
            case ir::TypeCategory::BUILTIN:
            case ir::TypeCategory::ENUM:
            case ir::TypeCategory::BITFIELD:
            case ir::TypeCategory::ALIGNED:
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Debug info emission is not implemented for type `{}`", type_expression->ir_code()));
        }
        __builtin_unreachable();
    }

    std::shared_ptr<BooleanTypeEntry> DebugInfo::add_boolean_type_entry(std::shared_ptr<ir::BooleanType> type_expression) {
        auto entry = std::make_shared<BooleanTypeEntry> (type_expression->name, type_expression->size_bits, type_expression->location);
        types[type_expression] = entry;
        return entry;
    }

    std::shared_ptr<IntegerTypeEntry> DebugInfo::add_integer_type_entry(std::shared_ptr<ir::IntegerType> type_expression) {
        auto entry = std::make_shared<IntegerTypeEntry> (type_expression->name, type_expression->is_signed(), type_expression->size_bits, type_expression->location);
        types[type_expression] = entry;
        return entry;
    }

    std::shared_ptr<PointerTypeEntry> DebugInfo::add_pointer_type_entry(std::shared_ptr<ir::PointerType> type_expression) {
        // Defense against recursive type : add the entry shell to the map first, then resolve the referenced type
        auto entry = std::make_shared<PointerTypeEntry> (nullptr, type_expression->location);
        types[type_expression] = entry;

        // For void*, generate a pointer type entry without referenced type
        if (type_expression->referenced_type->category != ir::TypeCategory::VOID)
            entry->referenced_type = type(type_expression->referenced_type);
        return entry;
    }

    std::shared_ptr<ArrayTypeEntry> DebugInfo::add_array_type_entry(std::shared_ptr<ir::ArrayType> type_expression) {
        std::optional<size_t> size = {};
        if (type_expression->length.tag() == ir::Operand::CONSTANT && type_expression->length.constant().tag() == ir::Constant::INTEGER)
            size = static_cast<size_t>(type_expression->length.constant().integer());

        // Not sure if that's even possible with array types, but anyways defense against recursive type : add the entry shell to the map first, then resolve the element type
        auto entry = std::make_shared<ArrayTypeEntry> (nullptr, size, type_expression->location);
        types[type_expression] = entry;

        entry->element_type = type(type_expression->element_type);
        return entry;
    }

    std::shared_ptr<CompoundTypeEntry> DebugInfo::add_struct_type_entry(std::shared_ptr<ir::StructType> type_expression) {
        std::optional<std::string> name = {};
        if (type_expression->name[0] != '.')  // Skip the generated name of anonymous structs
            name = type_expression->name;

        // Defense against recursive type : add the entry shell to the map first, then resolve the member types
        std::vector<std::shared_ptr<MemberEntry>> members;
        auto entry = std::make_shared<CompoundTypeEntry> (Tag::DW_TAG_structure_type, type_expression->size(type_expression->location), name, members, type_expression->location);
        types[type_expression] = entry;

        for (const auto& [index, member] : std::ranges::enumerate_view(type_expression->members))
            entry->children.push_back(std::make_shared<MemberEntry>(member.name, type_expression->member_offset(index), type(member.type), member.location));

        return entry;
    }

    std::shared_ptr<CompoundTypeEntry> DebugInfo::add_union_type_entry(std::shared_ptr<ir::UnionType> type_expression) {
        std::optional<std::string> name = {};
        if (type_expression->name[0] != '.')  // Skip the generated name of anonymous structs
            name = type_expression->name;

        // Defense against recursive type : add the entry shell to the map first, then resolve the member types
        std::vector<std::shared_ptr<MemberEntry>> members;
        auto entry = std::make_shared<CompoundTypeEntry> (Tag::DW_TAG_union_type, type_expression->size(type_expression->location), name, members, type_expression->location);
        types[type_expression] = entry;

        for (const auto& [index, member] : std::ranges::enumerate_view(type_expression->members))
            entry->children.push_back(std::make_shared<MemberEntry>(member.name, 0, type(member.type), member.location));

        return entry;
    }

    std::shared_ptr<QualifiedTypeEntry> DebugInfo::add_qualified_type_entry(std::shared_ptr<ir::QualifiedType> type_expression) {
        std::shared_ptr<TypeEntry> entry = type(type_expression->underlying_type);
        for (ir::TypeQualifier qualifier : type_expression->qualifiers) {
            switch (qualifier) {
                case ir::TypeQualifier::CONST:    entry = std::make_shared<QualifiedTypeEntry>(Tag::DW_TAG_const_type,    entry, type_expression->location);  break;
                case ir::TypeQualifier::VOLATILE: entry = std::make_shared<QualifiedTypeEntry>(Tag::DW_TAG_volatile_type, entry, type_expression->location);  break;
                case ir::TypeQualifier::RESTRICT: entry = std::make_shared<QualifiedTypeEntry>(Tag::DW_TAG_restrict_type, entry, type_expression->location);  break;
                case ir::TypeQualifier::ATOMIC:   entry = std::make_shared<QualifiedTypeEntry>(Tag::DW_TAG_atomic_type,   entry, type_expression->location);  break;
            }
        }

        types[type_expression] = entry;
        return std::static_pointer_cast<QualifiedTypeEntry>(entry);
    }

}
