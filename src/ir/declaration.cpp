#include <set>
#include <sstream>

#include "diagnostic.h"
#include "ir/declaration.h"

namespace toycc::ir {
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

    Member::Member(std::string name, std::shared_ptr<Type> type, CodeLocation location) : name(name), type(type), location(location) {}
    std::string Member::ir_code() const {
        return std::format("{} {}", type->ir_code(), name);
    }

    Declaration::Declaration(Member member, Flags<StorageClass> storage, Flags<FunctionSpecifier> function_spec)
        : Member(member), storage(storage), function_spec(function_spec) {}

    Declaration::Declaration(std::string name, std::shared_ptr<Type> type, CodeLocation location, Flags<StorageClass> storage, Flags<FunctionSpecifier> function_spec)
        : Member(name, type, location), storage(storage), function_spec(function_spec) {}

    // Throw diagnostics if the declaration's semantics are inconsistent
    static const std::set<Flags<StorageClass>> VALID_STORAGE_CLASSES = {StorageClass::AUTO, StorageClass::STATIC, StorageClass::EXTERN, StorageClass::REGISTER, StorageClass::TYPEDEF,
        StorageClass::THREAD_LOCAL | StorageClass::STATIC, StorageClass::THREAD_LOCAL | StorageClass::EXTERN};
    void Declaration::check() const {
        if (name.empty())
            throw Diagnostic(DiagnosticLevel::ERROR, "Unnamed declaration", location);

        if (!VALID_STORAGE_CLASSES.contains(storage))
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Invalid combination of storage class specifiers `{}`", storage_classes_repr(storage)), location);

        if (storage == StorageClass::TYPEDEF && function_spec)
            throw Diagnostic(DiagnosticLevel::ERROR, "Typedef declaration can't have function specifiers", location);
    }

    std::string Declaration::ir_code() const {
        return std::format("#decl {}{}{}", storage_classes_repr(storage), function_specifiers_repr(function_spec), Member::ir_code());
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
