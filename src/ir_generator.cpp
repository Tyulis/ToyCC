#include "diagnostic.h"
#include "ir_generator.h"
#include "ir/type.h"
#include "ir/declaration.h"
#include "ir/compound_type.h"
#include "arch/x86_64.h"

namespace toycc {
    void IRGenerator::add_primitive_type(std::string name, bool is_signed, ir::PrimitiveSemantic semantic, size_t size, size_t alignment) {
        ir::TypeIdentifier identifier = {.category = ir::TypeCategory::PRIMITIVE, .name = name};
        current_scope()->types[identifier] = std::make_shared<ir::PrimitiveType>(name, is_signed, semantic, size, alignment);
    }

    void IRGenerator::add_builtin_type(std::string name) {
        ir::TypeIdentifier identifier = {.category = ir::TypeCategory::BUILTIN, .name = name};
        CodeLocation location = {.filename = "<built-in>", .line = 1, .character = 1};
        std::shared_ptr<ir::Type> type_decl = std::make_shared<ir::Type> (identifier, location);
        current_scope()->types[identifier] = type_decl;

        // Built-in types will be identifier as typedef names in the syntax, define them as such
        std::shared_ptr<ir::Declaration> typedef_decl = std::make_shared<ir::Declaration>
                (ir::Declaration {.name = name, .location = location, .storage = ir::StorageClass::TYPEDEF, .spec = {}});
        typedef_decl->spec.type = type_decl;
        current_scope()->typedefs[name] = typedef_decl;
    }

    IRGenerator::IRGenerator(const SourceMap& source_map) : source_map(source_map) {
        // Initialize the global scope
        scope_stack.push_back(std::make_shared<ir::Scope>());

        using namespace toycc::arch;
        add_primitive_type("void",                   false, ir::PrimitiveSemantic::VOID,    0,                0);
        add_primitive_type("_Bool",                  false, ir::PrimitiveSemantic::BOOL,    BOOL_SIZE,        BOOL_ALIGNMENT);
        add_primitive_type("signed char",            true,  ir::PrimitiveSemantic::INTEGER, CHAR_SIZE,        CHAR_ALIGNMENT);
        add_primitive_type("unsigned char",          false, ir::PrimitiveSemantic::INTEGER, CHAR_SIZE,        CHAR_ALIGNMENT);
        add_primitive_type("signed short int",       true,  ir::PrimitiveSemantic::INTEGER, SHORT_SIZE,       SHORT_ALIGNMENT);
        add_primitive_type("unsigned short int",     false, ir::PrimitiveSemantic::INTEGER, SHORT_SIZE,       SHORT_ALIGNMENT);
        add_primitive_type("signed int",             true,  ir::PrimitiveSemantic::INTEGER, INT_SIZE,         INT_ALIGNMENT);
        add_primitive_type("unsigned int",           false, ir::PrimitiveSemantic::INTEGER, INT_SIZE,         INT_ALIGNMENT);
        add_primitive_type("signed long int",        true,  ir::PrimitiveSemantic::INTEGER, LONG_SIZE,        LONG_ALIGNMENT);
        add_primitive_type("unsigned long int",      false, ir::PrimitiveSemantic::INTEGER, LONG_SIZE,        LONG_ALIGNMENT);
        add_primitive_type("signed long long int",   false, ir::PrimitiveSemantic::INTEGER, LONG_LONG_SIZE,   LONG_LONG_ALIGNMENT);
        add_primitive_type("unsigned long long int", false, ir::PrimitiveSemantic::INTEGER, LONG_LONG_SIZE,   LONG_LONG_ALIGNMENT);
        add_primitive_type("float",                  true,  ir::PrimitiveSemantic::FLOAT,   FLOAT_SIZE,       FLOAT_ALIGNMENT);
        add_primitive_type("double",                 true,  ir::PrimitiveSemantic::FLOAT,   DOUBLE_SIZE,      DOUBLE_ALIGNMENT);
        add_primitive_type("long double",            true,  ir::PrimitiveSemantic::FLOAT,   LONG_DOUBLE_SIZE, LONG_DOUBLE_ALIGNMENT);

        add_builtin_type("__builtin_va_list");
    }

    // ------------ Listener

    void IRGenerator::exitDeclarationDeclaration(CParser::DeclarationDeclarationContext* context) {
        decode_declaration(context);
    }


    // ------------ IR generation functions

    // Decode a declaration and push it to the current scope
    void IRGenerator::decode_declaration(CParser::DeclarationDeclarationContext* context) {
        ir::Declaration base_declaration;
        decode_declaration_specifier(base_declaration, context->declarationSpecifiers());

        // First, only process the declarations
        std::vector<ir::Declaration> declarations;
        if (context->initDeclaratorList()) {
            for (CParser::InitDeclaratorContext* declarator : context->initDeclaratorList()->initDeclarator()) {
                declarations.push_back(base_declaration);
                decode_declarator(declarations.back(), declarator->declarator());
            }
        } else {
            declarations.push_back(base_declaration);
        }

        for (const ir::Declaration& declaration : declarations) {
            declaration.check(false);

            std::optional<CodeLocation> duplicate = get_name_location(declaration.name, true);
            if (duplicate.has_value())
                throw Diagnostic(Diagnostic::Level::ERROR, std::format("Attempted to redefine `{}`", declaration.name), declaration.location)
                       .add_note(Diagnostic::Level::NOTE,  "Previously defined here", duplicate.value());

            if (declaration.storage & ir::StorageClass::TYPEDEF)
                current_scope()->typedefs[declaration.name] = std::make_shared<ir::Declaration>(declaration);
            else
                current_scope()->locals[declaration.name] = std::make_shared<ir::Declaration>(declaration);
        }

        // Then the initializations
        if (context->initDeclaratorList()) {
            for (unsigned decl_index = 0; decl_index < declarations.size(); decl_index++) {
                const ir::Declaration& declaration = declarations[decl_index];
                CParser::InitDeclaratorContext* declarator = context->initDeclaratorList()->initDeclarator()[decl_index];
                if (declarator->initializer()) {
                    if (declaration.storage & ir::StorageClass::TYPEDEF)
                        throw Diagnostic(Diagnostic::Level::ERROR, "Initializers are not allowed in typedef declarations", get_location(declarator->initializer()));
                    throw Diagnostic(Diagnostic::Level::NOT_IMPLEMENTED, "Implement initializers here");
                }
            }
        }
    }

    void IRGenerator::decode_declaration_specifier(ir::Declaration& declaration, CParser::DeclarationSpecifiersContext* context) {
        std::vector<CParser::DeclarationSpecifierContext*> specifiers = context->declarationSpecifier();
        std::vector<CParser::TypeSpecifierContext*> type_specifiers;
        for (CParser::DeclarationSpecifierContext* specifier : specifiers) {
            if (specifier->storageClassSpecifier())
                declaration.storage |= decode_storage_class(specifier->storageClassSpecifier());
            else if (specifier->typeSpecifier())
                type_specifiers.push_back(specifier->typeSpecifier());
            else if (specifier->typeQualifier())
                declaration.spec.qualifiers |= decode_type_qualifier(specifier->typeQualifier());
            else if (specifier->functionSpecifier())
                declaration.spec.function_spec |= decode_function_specifier(specifier->functionSpecifier());
            else if (specifier->alignmentSpecifier())
                declaration.spec.custom_alignment = resolve_alignment_specifier(specifier->alignmentSpecifier());
            else
                throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, std::format("Unknown declaration specifier `{}`", specifier->getText()), get_location(context));
        }

        // In typedefs like `unsigned int my_type_t`, everything is in the type specifiers, which is otherwise invalid - split those
        const bool is_typedef = declaration.storage & ir::StorageClass::TYPEDEF;
        if (is_typedef && type_specifiers.back()->typedefName()) {
            declaration.name = type_specifiers.back()->typedefName()->Identifier()->getText();
            type_specifiers.pop_back();
        }

        ir::TypeSpecification initial_spec = resolve_type_specifier(type_specifiers, is_typedef);
        declaration.spec = initial_spec.merge(declaration.spec, get_location(context));
        declaration.check(false);
    }

    Flags<ir::StorageClass> IRGenerator::decode_storage_class(CParser::StorageClassSpecifierContext* context) {
        if      (context->Typedef())      return ir::StorageClass::TYPEDEF;
        else if (context->Extern())       return ir::StorageClass::EXTERN;
        else if (context->Static())       return ir::StorageClass::STATIC;
        else if (context->ThreadLocal())  return ir::StorageClass::THREAD_LOCAL;
        else if (context->Auto())         return ir::StorageClass::AUTO;
        else if (context->Register())     return ir::StorageClass::REGISTER;
        else throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, std::format("Unknown storage class `{}`", context->getText()), get_location(context));
    }

    Flags<ir::TypeQualifier> IRGenerator::decode_type_qualifier_list(CParser::TypeQualifierListContext* context) {
        Flags<ir::TypeQualifier> qualifiers;
        for (CParser::TypeQualifierContext* qualifier : context->typeQualifier())
            qualifiers |= decode_type_qualifier(qualifier);
        return qualifiers;
    }

    Flags<ir::TypeQualifier> IRGenerator::decode_type_qualifier(CParser::TypeQualifierContext* context) {
        if      (context->Const())     return ir::TypeQualifier::CONST;
        else if (context->Restrict())  return ir::TypeQualifier::RESTRICT;
        else if (context->Volatile())  return ir::TypeQualifier::VOLATILE;
        else if (context->Atomic())    return ir::TypeQualifier::ATOMIC;
        else throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, std::format("Unknown type qualifier `{}`", context->getText()), get_location(context));
    }

    Flags<ir::FunctionSpecifier> IRGenerator::decode_function_specifier(CParser::FunctionSpecifierContext* context) {
        if      (context->Inline())    return ir::FunctionSpecifier::INLINE;
        else if (context->Noreturn())  return ir::FunctionSpecifier::NORETURN;
        else if (context->gccAttributeSpecifier()) return {};  // Skip
        else throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, std::format("Unknown function specifier `{}`", context->getText()), get_location(context));
    }

    size_t IRGenerator::resolve_alignment_specifier(CParser::AlignmentSpecifierContext* context) {
        throw Diagnostic(Diagnostic::Level::NOT_IMPLEMENTED, "Alignment specifiers are not implemented", get_location(context));
    }

    ir::TypeSpecification IRGenerator::resolve_type_specifier(std::vector<CParser::TypeSpecifierContext*> type_specifiers, bool is_typedef) {
        const CodeLocation location = get_location(type_specifiers[0]);

        ir::TypeIdentifier identifier = decode_type_specifier(type_specifiers);
        std::optional<ir::TypeSpecification> spec = resolve_type(identifier);

        // Declare incomplete types in typedefs
        if (!spec.has_value() && is_typedef && (identifier.category == ir::TypeCategory::STRUCT || identifier.category == ir::TypeCategory::UNION || identifier.category == ir::TypeCategory::ENUM)) {
            current_scope()->types[identifier] = std::make_shared<ir::CompoundType>(identifier, location);
            spec = resolve_type(identifier);
        }

        if (!spec.has_value())
            throw Diagnostic(Diagnostic::Level::ERROR, std::format("Type `{}` is not defined", identifier.text()), location);
        return spec.value();
    }

    // Decode a type specifier, push eventual anonymous struct/enum/union declarations to the current scope and return the type identifier
    // Don't check whether non-primitive named types exists
    ir::TypeIdentifier IRGenerator::decode_type_specifier(std::vector<CParser::TypeSpecifierContext*> type_specifiers) {
        if (type_specifiers.empty())
            throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, "Empty type specifier list");

        const CodeLocation base_location = get_location(type_specifiers[0]);

        std::optional<std::string> sign_token;
        std::multiset<std::string> primitive_type_tokens;
        std::optional<ir::TypeIdentifier> identifier;

        for (CParser::TypeSpecifierContext* specifier : type_specifiers) {
            const std::string token = specifier->getText();
            const CodeLocation location = get_location(specifier);

            if (specifier->Void() || specifier->Char() || specifier->Bool() || specifier->Float() || specifier->Short() || specifier->Int() || specifier->Double() || specifier->Long()) {
                primitive_type_tokens.insert(token);
            } else if (specifier->Signed() || specifier->Unsigned()) {
                if (sign_token.has_value())
                    throw Diagnostic(Diagnostic::Level::ERROR, "Declaration can't have more than one sign specifier", location);
                sign_token = token;
            } else if (specifier->typedefName()) {
                if (identifier.has_value())
                    throw Diagnostic(Diagnostic::Level::ERROR, "Non-primitive types can't have more than one identifier", location);
                identifier = ir::TypeIdentifier {.category = ir::TypeCategory::TYPEDEF, .name = token};
            }
            else if (specifier->Complex())                throw Diagnostic(Diagnostic::Level::NOT_IMPLEMENTED, "Complex types are unsupported",                get_location(specifier));
            else if (specifier->atomicTypeSpecifier())    throw Diagnostic(Diagnostic::Level::NOT_IMPLEMENTED, "Atomic type specifiers are unsupported",       get_location(specifier));
            else if (specifier->structOrUnionSpecifier()) {
                if (identifier.has_value())
                    throw Diagnostic(Diagnostic::Level::ERROR, "Non-primitive types can't have more than one identifier", location);
                identifier = decode_struct_or_union_specifier(specifier->structOrUnionSpecifier());
            }
            else if (specifier->enumSpecifier())          throw Diagnostic(Diagnostic::Level::NOT_IMPLEMENTED, "Enums are unsupported",                        get_location(specifier));
            else throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, std::format("Unknown type specifier `{}`", specifier->getText()), get_location(specifier));
        }

        if (!identifier.has_value() && !sign_token.has_value() && primitive_type_tokens.empty())
            throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, "No valid type specifier found", base_location);

        if (identifier.has_value()) {
            if (sign_token.has_value() || !primitive_type_tokens.empty())
                throw Diagnostic(Diagnostic::Level::ERROR, "Non-primitive type name can't have primitive type specifiers", base_location);
            return identifier.value();
        }

        // ---- Now resolve primitive types. From now on `typedef_token` is empty

        // Void and bool are always alone
        if (primitive_type_tokens.contains("_Bool") || primitive_type_tokens.contains("void")) {
            if (primitive_type_tokens.size() > 1 || sign_token.has_value())
                throw Diagnostic(Diagnostic::Level::ERROR, "Primitive types _Bool and void can't have other type specifiers", base_location);
            return {ir::TypeCategory::PRIMITIVE, *primitive_type_tokens.begin()};
        }

        // Floating-point types
        if (primitive_type_tokens.contains("float") || primitive_type_tokens.contains("double")) {
            if (sign_token.has_value())
                throw Diagnostic(Diagnostic::Level::ERROR, "Floating-point type can't have a sign specifier", base_location);

            if (primitive_type_tokens.contains("float")) {
                if (primitive_type_tokens.size() > 1)
                    throw Diagnostic(Diagnostic::Level::ERROR, "Primitive type `float` can't have any other type specifiers", base_location);
                return {ir::TypeCategory::PRIMITIVE, "float"};
            }

            // From now on, only double remains
            if (primitive_type_tokens.contains("long") && primitive_type_tokens.size() == 2)
                return {ir::TypeCategory::PRIMITIVE, "long double"};
            else if (primitive_type_tokens.size() == 1)
                return {ir::TypeCategory::PRIMITIVE, "double"};
            else
                throw Diagnostic(Diagnostic::Level::ERROR, "Primitive type `double` can't have any type specifiers other than `long`", base_location);
        }

        // From now on, only integer types remain and sign_token has a value
        // signed / unsigned without a type -> implied int; type without a sign qualifier -> implied signed
        if (sign_token.has_value() && primitive_type_tokens.empty())
            primitive_type_tokens.insert("int");
        else if (!sign_token.has_value() && !primitive_type_tokens.empty())
            sign_token = "signed";

        if (primitive_type_tokens.count("int") > 1)
            throw Diagnostic(Diagnostic::Level::ERROR, "Primitive type can't specify `int` more than once", base_location);

        if (primitive_type_tokens.contains("char")) {
            if (primitive_type_tokens.size() > 1)
                throw Diagnostic(Diagnostic::Level::ERROR, "Primitive type `char` can't have type specifiers other than `signed`/`unsigned`", base_location);
            return {ir::TypeCategory::PRIMITIVE, std::format("{} char", sign_token.value())};
        }

        // From now on, only short, int and long remain
        if (primitive_type_tokens.contains("short")) {
            if (primitive_type_tokens.contains("long"))
                throw Diagnostic(Diagnostic::Level::ERROR, "Primitive type `short` can't also be `long`", base_location);
            return {ir::TypeCategory::PRIMITIVE, std::format("{} short int", sign_token.value())};
        }

        // From now on, only long and int remain
        const size_t nof_longs = primitive_type_tokens.count("long");
        switch (nof_longs) {
            case 0:  return {ir::TypeCategory::PRIMITIVE, std::format("{} int", sign_token.value())};
            case 1:  return {ir::TypeCategory::PRIMITIVE, std::format("{} long int", sign_token.value())};
            case 2:  return {ir::TypeCategory::PRIMITIVE, std::format("{} long long int", sign_token.value())};
            default: throw Diagnostic(Diagnostic::Level::ERROR, "Primitive type specifier `long` can't be given more than twice", base_location);
        }
    }

    // Decode a struct or union specifier, push struct/enum/union definitions to the current scope and return the type identifier
    // Don't check whether named struct / unions exists
    ir::TypeIdentifier IRGenerator::decode_struct_or_union_specifier(CParser::StructOrUnionSpecifierContext* context) {
        const CodeLocation location = get_location(context);
        ir::TypeIdentifier identifier;

        if (context->Identifier()) identifier.name = context->Identifier()->getText();
        else                       identifier.name = anonymous_identifier();

        if      (context->structOrUnion()->Struct())  identifier.category = ir::TypeCategory::STRUCT;
        else if (context->structOrUnion()->Union())   identifier.category = ir::TypeCategory::UNION;
        else throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, std::format("Unknown struct or union keyword `{}`", context->structOrUnion()->getText()), location);

        if (context->structDeclarationList()) {
            std::shared_ptr<ir::CompoundType> definition = std::make_shared<ir::CompoundType>(identifier, location);
            current_scope()->types[identifier] = definition;  // Push the incomplete type to the current scope to allow struct members to use it

            for (CParser::StructDeclarationContext* declaration : context->structDeclarationList()->structDeclaration()) {
                throw Diagnostic(Diagnostic::Level::NOT_IMPLEMENTED, "Complete structure declarations are not implemented", get_location(declaration));
            }
        }

        return identifier;
    }

    void IRGenerator::decode_declarator(ir::Declaration& declaration, CParser::DeclaratorContext* context) {
        if (context->pointer())
            declaration.spec.pointer_spec = decode_pointer_spec(context->pointer());

        decode_direct_declarator(declaration, context->directDeclarator());

        if (!context->gccDeclaratorExtension().empty())
            throw Diagnostic(Diagnostic::Level::NOT_IMPLEMENTED, "GCC declarator extensions are not supported", get_location(context));
    }

    void IRGenerator::decode_direct_declarator(ir::Declaration& declaration, CParser::DirectDeclaratorContext* context) {
        if (CParser::DirectDeclaratorIdentifierContext::is(context)) {
            declaration.name = static_cast<CParser::DirectDeclaratorIdentifierContext*>(context)->Identifier()->getText();
        } else if (CParser::DirectDeclaratorParenthesizedContext::is(context)) {
            decode_declarator(declaration, static_cast<CParser::DirectDeclaratorParenthesizedContext*>(context)->declarator());
        } else if (CParser::DirectDeclaratorBitFieldContext::is(context)) {
            CParser::DirectDeclaratorBitFieldContext* declarator = static_cast<CParser::DirectDeclaratorBitFieldContext*>(context);
            declaration.name = declarator->Identifier()->getText();
            declaration.spec.bitfield_length = std::stoll(declarator->DigitSequence()->getText());
        } else if (CParser::DirectDeclaratorVCExtensionContext::is(context)) {
            throw Diagnostic(Diagnostic::Level::NOT_IMPLEMENTED, "VC declarator extensions are not supported", get_location(context));
        } else {
            throw Diagnostic(Diagnostic::Level::NOT_IMPLEMENTED, "This type of declarator is not implemented", get_location(context));
        }
    }

    void IRGenerator::decode_initializer(ir::Declaration const&, CParser::InitializerContext* context) {
        throw Diagnostic(Diagnostic::Level::NOT_IMPLEMENTED, "Initializers are not implemented", get_location(context));
    }

    std::vector<Flags<ir::TypeQualifier>> IRGenerator::decode_pointer_spec(CParser::PointerContext* context) {
        std::vector<Flags<ir::TypeQualifier>> pointer_spec(context->pointerLevel().size());
        for (CParser::PointerLevelContext* level : context->pointerLevel()) {
            if (level->Caret())
                throw Diagnostic(Diagnostic::Level::NOT_IMPLEMENTED, "Carets in pointer specification are not supported", get_location(level));

            if (level->typeQualifierList())
                pointer_spec.push_back(decode_type_qualifier_list(level->typeQualifierList()));
            else
                pointer_spec.emplace_back();
        }

        return pointer_spec;
    }

    // ------------ Internals
    std::shared_ptr<ir::Scope> IRGenerator::current_scope() {
        return scope_stack.back();
    }

    std::optional<CodeLocation> IRGenerator::get_name_location(std::string name, bool current_scope_only) {
        ir::TypeIdentifier type_identifier = {.category = ir::TypeCategory::PRIMITIVE, .name = name};
        for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); it++) {
            std::shared_ptr<ir::Scope> scope = *it;
            if (scope->types.contains(type_identifier))
                return scope->types.at(type_identifier)->location;
            else if (scope->locals.contains(name))
                return scope->locals.at(name)->location;
            else if (scope->typedefs.contains(name))
                return scope->typedefs.at(name)->location;
            // Structs, unions and enums aren't single names, they have `struct` / `union` / `enum` in front

            if (current_scope_only)
                break;
        }

        return {};
    }

    CodeLocation IRGenerator::get_location(antlr4::ParserRuleContext* context) const {
        antlr4::Token* start_token = context->getStart();
        LinePosition line = source_map.at(start_token->getLine());
        return {.filename = line.filename, .line = line.line, .character = start_token->getCharPositionInLine()};
    }

    std::string IRGenerator::anonymous_identifier() {
        return std::format("<anonymous_{}>", unique_id++);
    }

    std::optional<ir::TypeSpecification> IRGenerator::resolve_type(ir::TypeIdentifier identifier) {
        for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); it++) {
            std::shared_ptr<ir::Scope> scope = *it;
            if (identifier.category == ir::TypeCategory::TYPEDEF) {
                if (scope->typedefs.contains(identifier.name)) {
                    std::shared_ptr<ir::Declaration> typedef_decl = scope->typedefs.at(identifier.name);
                    return typedef_decl->spec;
                }
            } else {
                if (scope->types.contains(identifier)) {
                    ir::TypeSpecification spec;
                    spec.type = scope->types.at(identifier);
                    return spec;
                }
            }
        }

        return {};
    }
}
