#include <set>

#include "diagnostic.h"
#include "ir/declaration.h"

namespace toycc::ir {
    // Throw diagnostics if the specifications's semantics are inconsistent
    void TypeSpecification::check(bool in_struct, CodeLocation location) const {
        if (type.name.empty())
            throw Diagnostic(Diagnostic::Level::ERROR, "No base type name in declaration", location);
        if (!in_struct && bitfield_length.has_value())
            throw Diagnostic(Diagnostic::Level::ERROR, "Bitfield types can't appear outside of structure declarations", location);
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
}
