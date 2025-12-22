#include <set>

#include "diagnostic.h"
#include "ir/declaration.h"

namespace toycc::ir {
    // Throw diagnostics if the specifications's semantics are inconsistent
    void TypeSpecification::check(bool in_struct, CodeLocation location) const {
        if (type == nullptr)
            throw Diagnostic(Diagnostic::Level::ERROR, "No base type name in declaration", location);
        if (!in_struct && bitfield_length.has_value())
            throw Diagnostic(Diagnostic::Level::ERROR, "Bitfield types can't appear outside of structure declarations", location);
        if (bitfield_length.has_value() && type->identifier.category != TypeCategory::PRIMITIVE)
            throw Diagnostic(Diagnostic::Level::ERROR, "Bitfields must have a primitive type", location);
    }

    // Throw diagnostics if the declaration's semantics are inconsistent
    static const std::set<Flags<StorageClass>> VALID_STORAGE_CLASSES = {StorageClass::AUTO, StorageClass::STATIC, StorageClass::EXTERN, StorageClass::REGISTER, StorageClass::TYPEDEF,
                                                                        StorageClass::THREAD_LOCAL | StorageClass::STATIC, StorageClass::THREAD_LOCAL | StorageClass::EXTERN};
    void Declaration::check(bool is_struct) const {
        spec.check(is_struct, location);

        if (name.empty())
            throw Diagnostic(Diagnostic::Level::ERROR, "Unnamed declaration", location);

        if (!VALID_STORAGE_CLASSES.contains(storage))
            throw Diagnostic(Diagnostic::Level::ERROR, "Invalid combination of storage class specifiers", location);

        if (storage == StorageClass::TYPEDEF && spec.function_spec)
            throw Diagnostic(Diagnostic::Level::ERROR, "Typedef declaration can't have function specifiers", location);
    }

    TypeSpecification TypeSpecification::merge(TypeSpecification overriding, CodeLocation location) const {
        TypeSpecification merged = *this;

        if (merged.type == nullptr)
            merged.type = overriding.type;
        else if (overriding.type != nullptr)
            throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, "Attempted to override the type of a type spec that already has a valid type", location);

        merged.qualifiers |= overriding.qualifiers;
        merged.pointer_spec.append_range(overriding.pointer_spec);
        merged.array_spec.append_range(overriding.array_spec);
        merged.function_spec |= overriding.function_spec;

        if (!merged.prototype.has_value())
            merged.prototype = overriding.prototype;
        else if (overriding.prototype.has_value())
            throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, "Attempted to override the prototype of a type spec that already has a prototype", location);

        if (!merged.custom_alignment.has_value())
            merged.custom_alignment = overriding.custom_alignment;
        else if (!overriding.custom_alignment.has_value())
            throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, "Attempted to override the alignment of a type spec that already has an alignment", location);

        if (!merged.bitfield_length.has_value())
            merged.bitfield_length = overriding.bitfield_length;
        else if (!overriding.bitfield_length.has_value())
            throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, "Attempted to override the bitfield length of a type spec that already has a bitfield length", location);

        return merged;
    }
}
