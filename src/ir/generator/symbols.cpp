#include "diagnostic.h"
#include "ir/generator.h"
#include "arch/x86_64.h"

namespace toycc::ir {
    void Generator::init_global_scope() {
        // Initialize the global scope
        scope_stack.push_back(std::make_shared<Scope>(ScopeType::GLOBAL, nullptr));
        current_scope()->function = nullptr;

        using namespace toycc::arch;
        TypeIdentifier void_identifier = {.category = TypeCategory::VOID, .name = "void"};
        current_scope()->add_type(std::make_shared<Type> (void_identifier, CodeLocation {.filename = "<built-in>", .line = 1, .character = 1}));

        add_primitive_type("bool",                   false, PrimitiveSemantic::BOOL,    BOOL_SIZE,        BOOL_ALIGNMENT);
        add_primitive_type("signed char",            true,  PrimitiveSemantic::INTEGER, CHAR_SIZE,        CHAR_ALIGNMENT);
        add_primitive_type("unsigned char",          false, PrimitiveSemantic::INTEGER, CHAR_SIZE,        CHAR_ALIGNMENT);
        add_primitive_type("signed short int",       true,  PrimitiveSemantic::INTEGER, SHORT_SIZE,       SHORT_ALIGNMENT);
        add_primitive_type("unsigned short int",     false, PrimitiveSemantic::INTEGER, SHORT_SIZE,       SHORT_ALIGNMENT);
        add_primitive_type("signed int",             true,  PrimitiveSemantic::INTEGER, INT_SIZE,         INT_ALIGNMENT);
        add_primitive_type("unsigned int",           false, PrimitiveSemantic::INTEGER, INT_SIZE,         INT_ALIGNMENT);
        add_primitive_type("signed long int",        true,  PrimitiveSemantic::INTEGER, LONG_SIZE,        LONG_ALIGNMENT);
        add_primitive_type("unsigned long int",      false, PrimitiveSemantic::INTEGER, LONG_SIZE,        LONG_ALIGNMENT);
        add_primitive_type("signed long long int",   false, PrimitiveSemantic::INTEGER, LONG_LONG_SIZE,   LONG_LONG_ALIGNMENT);
        add_primitive_type("unsigned long long int", false, PrimitiveSemantic::INTEGER, LONG_LONG_SIZE,   LONG_LONG_ALIGNMENT);
        add_primitive_type("float",                  true,  PrimitiveSemantic::FLOAT,   FLOAT_SIZE,       FLOAT_ALIGNMENT);
        add_primitive_type("double",                 true,  PrimitiveSemantic::FLOAT,   DOUBLE_SIZE,      DOUBLE_ALIGNMENT);
        add_primitive_type("long double",            true,  PrimitiveSemantic::FLOAT,   LONG_DOUBLE_SIZE, LONG_DOUBLE_ALIGNMENT);

        add_builtin_type("__builtin_va_list");
    }

    void Generator::add_builtin_type(std::string name) {
        TypeIdentifier identifier = {.category = TypeCategory::BUILTIN, .name = name};
        CodeLocation location = {.filename = "<built-in>", .line = 1, .character = 1};
        std::shared_ptr<Type> type_decl = std::make_shared<Type> (identifier, location);
        current_scope()->add_type(type_decl);

        // Built-in types will be identified as typedef names in the syntax, define them as such
        std::shared_ptr<Declaration> typedef_decl = std::make_shared<Declaration>
        (Declaration {.name = name, .location = location, .storage = StorageClass::TYPEDEF, .spec = {}});
        typedef_decl->spec.type = type_decl;
        current_scope()->add_typedef(typedef_decl);
    }

    void Generator::add_primitive_type(std::string name, bool is_signed, PrimitiveSemantic semantic, size_t size, size_t alignment) {
        TypeIdentifier identifier = {.category = TypeCategory::PRIMITIVE, .name = name};
        current_scope()->add_type(std::make_shared<PrimitiveType>(name, is_signed, semantic, size, alignment));
    }


    std::shared_ptr<Scope> Generator::create_function_scope(std::shared_ptr<Declaration> declaration) {
        std::shared_ptr<Scope> scope = std::make_shared<Scope>(ScopeType::FUNCTION, declaration);
        for (const Declaration& parameter : declaration->spec.parameters)
            if (!parameter.name.empty())
                scope->add_local(std::make_shared<Declaration>(parameter));

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


    std::shared_ptr<Declaration> Generator::declare_temporary(TypeSpecification spec, CodeLocation location) {
        Declaration declaration = {.name = anonymous_identifier(), .location = location, .storage = StorageClass::AUTO | StorageClass::TEMPORARY, .spec = spec};
        return declare(declaration);
    }


    std::optional<CodeLocation> Generator::locate_name(std::string name, bool current_scope_only) {
        std::vector<TypeIdentifier> type_identifiers = {{.category = TypeCategory::PRIMITIVE, .name = name},
        {.category = TypeCategory::BUILTIN,   .name = name},
        {.category = TypeCategory::VOID,      .name = name}};
        // Structs, unions and enums aren't single names, they have `struct` / `union` / `enum` in front

        for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); it++) {
            std::shared_ptr<Scope> scope = *it;

            for (TypeIdentifier identifier :type_identifiers) {
                std::shared_ptr<Type> type = scope->find_type(identifier);
                if (type.get() != nullptr)
                    return type->location;
            }

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

    std::optional<TypeSpecification> Generator::resolve_type_without_error(TypeIdentifier identifier) {
        for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); it++) {
            std::shared_ptr<Scope> scope = *it;
            if (identifier.category == TypeCategory::TYPEDEF) {
                std::shared_ptr<Declaration> typedef_decl = scope->find_typedef(identifier.name);
                if (typedef_decl.get() != nullptr)
                    return typedef_decl->spec;
            } else {
                std::shared_ptr<Type> type = scope->find_type(identifier);
                if (type.get() != nullptr) {
                    TypeSpecification spec;
                    spec.type = type;
                    return spec;
                }
            }
        }

        return {};
    }

    TypeSpecification Generator::resolve_type(TypeIdentifier identifier, CodeLocation location) {
        std::optional<TypeSpecification> spec = resolve_type_without_error(identifier);
        if (!spec.has_value())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Type `{}` was not declared", identifier.text()), location);
        return spec.value();
    }
}
