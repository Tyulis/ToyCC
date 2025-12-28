#include <set>
#include <sstream>

#include "diagnostic.h"
#include "ir/declaration.h"
#include "util/strings.h"

namespace toycc::ir {
    // Throw diagnostics if the specifications's semantics are inconsistent
    void TypeSpecification::check(bool in_struct, CodeLocation location) const {
        if (type == nullptr)
            throw Diagnostic(DiagnosticLevel::ERROR, "No base type name in declaration", location);
        if (!in_struct && bitfield_length.has_value())
            throw Diagnostic(DiagnosticLevel::ERROR, "Bitfield types can't appear outside of structure declarations", location);
        if (bitfield_length.has_value() && type->identifier.category != TypeCategory::PRIMITIVE)
            throw Diagnostic(DiagnosticLevel::ERROR, "Bitfields must have a primitive type", location);
        if (!is_function_type && function_spec)
            throw Diagnostic(DiagnosticLevel::ERROR, "Non-function declaration can't have function specifiers", location);
        if (!is_function_type && !parameters.empty())
            throw Diagnostic(DiagnosticLevel::ERROR, "Non-function declaration can't have function parameters", location);
    }

    TypeSpecification TypeSpecification::merge(TypeSpecification overriding, CodeLocation location) const {
        TypeSpecification merged = *this;

        if (merged.type == nullptr)
            merged.type = overriding.type;
        else if (overriding.type != nullptr)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to override the type of a type spec that already has a valid type", location);

        merged.qualifiers |= overriding.qualifiers;
        merged.pointer_spec.append_range(overriding.pointer_spec);
        merged.array_spec.append_range(overriding.array_spec);
        merged.function_spec |= overriding.function_spec;

        if (merged.parameters.empty())
            merged.parameters = overriding.parameters;
        else if (!overriding.parameters.empty())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to override the prototype of a type spec that already has a prototype", location);

        if (!merged.custom_alignment.has_value())
            merged.custom_alignment = overriding.custom_alignment;
        else if (!overriding.custom_alignment.has_value())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to override the alignment of a type spec that already has an alignment", location);

        if (!merged.bitfield_length.has_value())
            merged.bitfield_length = overriding.bitfield_length;
        else if (!overriding.bitfield_length.has_value())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to override the bitfield length of a type spec that already has a bitfield length", location);

        return merged;
    }

    bool TypeSpecification::is_void() const {
        return type->identifier.category == TypeCategory::VOID && pointer_spec.empty() && !is_function_type;
    }

    bool TypeSpecification::is_array_type() const {
        return !array_spec.empty();
    }

    bool TypeSpecification::is_pointer_type() const {
        return !pointer_spec.empty() || is_function_type;
    }

    // Object type, so not a pointer, function or void
    bool TypeSpecification::is_object_type() const {
        return !is_void() && !is_pointer_type();
    }

    // Get the type of the elements of an array type
    TypeSpecification TypeSpecification::element_type() const {
        if (!is_array_type())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to get the element type of a non-array type");

        return TypeSpecification {.array_spec = {},
                                  .is_function_type = is_function_type, .function_spec = function_spec, .parameters = parameters,
                                  .pointer_spec = pointer_spec,
                                  .type = type, .qualifiers = qualifiers, .custom_alignment = custom_alignment, .bitfield_length = bitfield_length};
    }

    // Get the return type of a function declaration
    TypeSpecification TypeSpecification::return_type() const {
        if (!is_function_type)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to get the return type of a non-function type");

        return TypeSpecification {.array_spec = {},
                                  .is_function_type = false, .function_spec = {}, .parameters = {},
                                  .pointer_spec = pointer_spec,
                                  .type = type, .qualifiers = qualifiers, .custom_alignment = custom_alignment, .bitfield_length = bitfield_length};
    }

    // Get the type referenced by a pointer
    TypeSpecification TypeSpecification::referenced_type() const {
        if (!is_pointer_type())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to get the referenced type of a non-pointer type");

        std::vector<Flags<TypeQualifier>> dereferenced_pointer_spec = pointer_spec;
        dereferenced_pointer_spec.pop_back();

        return TypeSpecification {.array_spec = {},
                                  .is_function_type = false, .function_spec = {}, .parameters = {},
                                  .pointer_spec = dereferenced_pointer_spec,
                                  .type = type, .qualifiers = qualifiers, .custom_alignment = custom_alignment, .bitfield_length = bitfield_length};
    }

    // Check whether `spec` is the exact same type
    bool TypeSpecification::operator== (const TypeSpecification& spec) const {
        return (type.get() == spec.type.get()
             && qualifiers == spec.qualifiers
             && pointer_spec == spec.pointer_spec
             && array_spec == spec.array_spec
             && is_function_type == spec.is_function_type
             && (!is_function_type || (function_spec == spec.function_spec && parameters == spec.parameters))
             && custom_alignment == spec.custom_alignment
             && bitfield_length == spec.bitfield_length);
    }

    // Check whether `spec` can be assigned directly to this type without conversion
    bool TypeSpecification::can_be_assigned_from(const TypeSpecification& spec) const {
        return (type.get() == spec.type.get()
             && pointer_spec == spec.pointer_spec
             && array_spec == spec.array_spec
             && is_function_type == spec.is_function_type
             && (!is_function_type || (function_spec == spec.function_spec && parameters == spec.parameters))
             && bitfield_length == spec.bitfield_length);
    }

    static std::string type_qualifiers_repr(Flags<TypeQualifier> qualifiers) {
        if (!qualifiers)
            return "";

        std::stringstream repr;
        if (qualifiers & TypeQualifier::CONST)     repr << "const ";
        if (qualifiers & TypeQualifier::VOLATILE)  repr << "volatile ";
        if (qualifiers & TypeQualifier::RESTRICT)  repr << "restrict ";
        if (qualifiers & TypeQualifier::ATOMIC)    repr << "atomic ";
        return repr.str();
    }

    static std::string function_specifiers_repr(Flags<FunctionSpecifier> specifiers) {
        if (!specifiers)
            return "";

        std::stringstream repr;
        if (specifiers & FunctionSpecifier::INLINE)    repr << "inline ";
        if (specifiers & FunctionSpecifier::NORETURN)  repr << "noreturn ";
        return repr.str();
    }

    static std::string storage_classes_repr(Flags<StorageClass> storage) {
        if (!storage)
            return "";

        std::stringstream repr;
        if (storage & StorageClass::AUTO)          repr << "auto ";
        if (storage & StorageClass::STATIC)        repr << "static ";
        if (storage & StorageClass::EXTERN)        repr << "extern ";
        if (storage & StorageClass::REGISTER)      repr << "register ";
        if (storage & StorageClass::THREAD_LOCAL)  repr << "thread_local ";
        if (storage & StorageClass::TYPEDEF)       repr << "typedef ";
        if (storage & StorageClass::PARAMETER)     repr << "parameter ";
        if (storage & StorageClass::TEMPORARY)     repr << "temporary ";
        if (storage & StorageClass::ADDRESSED)     repr << "addressed ";
        return repr.str();
    }

    std::string TypeSpecification::ir_code() const {
        std::stringstream code;
        if (custom_alignment.has_value())
            code << "alignas(" << custom_alignment.value() << ") ";
        code << function_specifiers_repr(function_spec) << "(" << type_qualifiers_repr(qualifiers) << type->identifier.ir_code();
        for (size_t level = 0; level < pointer_spec.size(); level++) {
            code << "*" << type_qualifiers_repr(pointer_spec[level]);
            if (level != pointer_spec.size() - 1)
                code << " ";
        }
        code << ")";

        if (is_function_type) {
            code << "(";
            if (parameters.size() > 0)
                code << "\n";
            for (const Declaration& parameter : parameters)
                code << "    " << parameter.ir_code() << "\n";
            code << ")";
        }

        for (std::shared_ptr<Declaration> size : array_spec)
            code << "[" << size->name << "]";

        if (bitfield_length.has_value())
            code << " : " << bitfield_length.value();

        return trim(code.str());
    }


    // Throw diagnostics if the declaration's semantics are inconsistent
    static const std::set<Flags<StorageClass>> VALID_STORAGE_CLASSES = {StorageClass::AUTO, StorageClass::STATIC, StorageClass::EXTERN, StorageClass::REGISTER, StorageClass::TYPEDEF,
        StorageClass::THREAD_LOCAL | StorageClass::STATIC, StorageClass::THREAD_LOCAL | StorageClass::EXTERN};
    void Declaration::check(bool is_struct) const {
        spec.check(is_struct, location);

        if (name.empty())
            throw Diagnostic(DiagnosticLevel::ERROR, "Unnamed declaration", location);

        if (!VALID_STORAGE_CLASSES.contains(storage))
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Invalid combination of storage class specifiers `{}`", storage_classes_repr(storage)), location);

        if (storage == StorageClass::TYPEDEF && spec.function_spec)
            throw Diagnostic(DiagnosticLevel::ERROR, "Typedef declaration can't have function specifiers", location);
    }

    std::string Declaration::ir_code() const {
        return std::format("#decl {}{} : {}", storage_classes_repr(storage), name, spec.ir_code());
    }

    bool Declaration::operator== (const Declaration& decl) const {
        return (name == decl.name && storage == decl.storage && spec == decl.spec);
    }


    std::string LValue::ir_code() const {
        std::stringstream code;
        code << base_declaration->name;
        for (std::shared_ptr<Declaration> index : indices) {
            if (index.get() == nullptr)
                code << "[0]";
            else
                code << "[" << index->name << "]";
        }
        return code.str();
    }
}
