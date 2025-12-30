#include "diagnostic.h"
#include "ir/generator.h"
#include "arch/datamodel.h"

namespace toycc::ir {
    void Generator::init_global_scope() {
        // Initialize the global scope
        scope_stack.push_back(std::make_shared<Scope>(ScopeType::GLOBAL, nullptr));
        current_scope()->function = nullptr;

        using namespace toycc::arch;
        current_scope()->add_type(std::make_shared<Type> (TypeCategory::VOID, std::string("void"), BUILTIN_LOCATION));
        boolean_type = current_scope()->add_type(std::make_shared<BooleanType> ("bool", BUILTIN_LOCATION, 8 * arch::DATAMODEL->bool_size(), 8 * arch::DATAMODEL->bool_alignment()));

        character_type = add_integer_type("signed char", true, arch::DATAMODEL->char_size(),   arch::DATAMODEL->char_alignment());
        add_integer_type("unsigned char",          false, arch::DATAMODEL->char_size(),        arch::DATAMODEL->char_alignment());
        add_integer_type("signed short int",       true,  arch::DATAMODEL->short_size(),       arch::DATAMODEL->short_alignment());
        add_integer_type("unsigned short int",     false, arch::DATAMODEL->short_size(),       arch::DATAMODEL->short_alignment());
        enum_underlying_type = add_integer_type("signed int", true, arch::DATAMODEL->int_size(), arch::DATAMODEL->int_alignment());
        add_integer_type("unsigned int",           false, arch::DATAMODEL->int_size(),         arch::DATAMODEL->int_alignment());
        add_integer_type("signed long int",        true,  arch::DATAMODEL->long_size(),        arch::DATAMODEL->long_alignment());
        add_integer_type("unsigned long int",      false, arch::DATAMODEL->long_size(),        arch::DATAMODEL->long_alignment());
        add_integer_type("signed long long int",   true,  arch::DATAMODEL->long_long_size(),   arch::DATAMODEL->long_long_alignment());
        add_integer_type("unsigned long long int", false, arch::DATAMODEL->long_long_size(),   arch::DATAMODEL->long_long_alignment());
        add_floating_point_type("float",                  arch::DATAMODEL->float_size(),       arch::DATAMODEL->float_alignment());
        add_floating_point_type("double",                 arch::DATAMODEL->double_size(),      arch::DATAMODEL->double_alignment());
        add_floating_point_type("long double",            arch::DATAMODEL->long_double_size(), arch::DATAMODEL->long_double_alignment());

        add_builtin_type("__builtin_va_list");
    }

    std::shared_ptr<Type> Generator::add_builtin_type(std::string name) {
        return current_scope()->add_type(std::make_shared<Type>(TypeCategory::BUILTIN, name, BUILTIN_LOCATION));
    }

    std::shared_ptr<Type> Generator::add_integer_type(std::string name, bool is_signed, size_t size, size_t alignment) {
        return current_scope()->add_type(std::make_shared<IntegerType>(name, BUILTIN_LOCATION, size * 8, alignment * 8, is_signed));
    }

    std::shared_ptr<Type> Generator::add_floating_point_type(std::string name, size_t size, size_t alignment) {
        return current_scope()->add_type(std::make_shared<FloatingPointType>(name, BUILTIN_LOCATION, size * 8, alignment * 8));
    }


    std::shared_ptr<Scope> Generator::create_function_scope(std::shared_ptr<Declaration> declaration) {
        std::shared_ptr<Scope> scope = std::make_shared<Scope>(ScopeType::FUNCTION, declaration);
        if (declaration->type->category != TypeCategory::FUNCTION)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to create the function scope for a non-function type");

        const FunctionType& function_type = static_cast<const FunctionType&> (*declaration->type);
        for (const Member& parameter : function_type.parameters)
            if (!parameter.name.empty())
                scope->add_local(std::make_shared<Declaration>(parameter, StorageClass::AUTO | StorageClass::PARAMETER));

        return scope;
    }

    std::shared_ptr<Declaration> Generator::declare(Declaration declaration) {
        std::optional<CodeLocation> existing_location = locate_name(declaration.name);
        if (existing_location.has_value())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Name {} was already declared", declaration.name), declaration.location)
                  .add_note(DiagnosticLevel::NOTE, "Previously declared here", existing_location.value());

        if (declaration.storage & StorageClass::TYPEDEF)
            return current_scope()->add_typedef(std::make_shared<Declaration>(declaration));
        else
            return current_scope()->add_local(std::make_shared<Declaration>(declaration));
    }


    std::shared_ptr<Declaration> Generator::declare_temporary(std::shared_ptr<Type> type, CodeLocation location) {
        Declaration declaration(anonymous_identifier(), type, location, StorageClass::AUTO | StorageClass::TEMPORARY);
        return declare(declaration);
    }

    std::optional<CodeLocation> Generator::locate_name(std::string name, bool current_scope_only) {
        const TypeIdentifier identifier = {.tag = TypeTag::DIRECT, .name = name};
        // Structs, unions and enums aren't single names, they have `struct` / `union` / `enum` in front

        for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); it++) {
            std::shared_ptr<Scope> scope = *it;

            std::shared_ptr<Type> type = scope->find_type(identifier);
            if (type.get() != nullptr)
                return type->location;

            std::shared_ptr<Declaration> typedef_decl = scope->find_typedef(name);
            if (typedef_decl.get() != nullptr)
                return typedef_decl->location;

            std::shared_ptr<Declaration> local_decl = scope->find_local(name);
            if (local_decl.get() != nullptr)
                return local_decl->location;

            if (current_scope_only)
                break;
        }

        return {};
    }

    std::shared_ptr<Declaration> Generator::resolve_without_error(std::string name) {
        for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); it++) {
            std::shared_ptr<Scope> scope = *it;
            std::shared_ptr<Declaration> local = scope->find_local(name);
            if (local.get() != nullptr)
                return local;
        }

        return nullptr;
    }

    std::shared_ptr<Declaration> Generator::resolve(std::string name, CodeLocation location) {
        std::shared_ptr<Declaration> declaration = resolve_without_error(name);

        if (declaration.get() == nullptr)
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Name `{}` was not declared", name), location);
        return declaration;
    }

    std::shared_ptr<Type> Generator::resolve_type_without_error(TypeIdentifier identifier) {
        for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); it++) {
            std::shared_ptr<Scope> scope = *it;
            if (identifier.tag == TypeTag::TYPEDEF) {
                std::shared_ptr<Declaration> typedef_decl = scope->find_typedef(identifier.name);
                if (typedef_decl.get() != nullptr)
                    return typedef_decl->type;
            } else {
                std::shared_ptr<Type> type = scope->find_type(identifier);
                if (type.get() != nullptr)
                    return type;
            }
        }

        return nullptr;
    }

    std::shared_ptr<Type> Generator::resolve_type(TypeIdentifier identifier, CodeLocation location) {
        std::shared_ptr<Type> type = resolve_type_without_error(identifier);
        if (type.get() == nullptr)
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Type `{}` was not declared", identifier.text()), location);
        return type;
    }
}
