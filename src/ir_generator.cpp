#include "TerminalNode.h"
#include "diagnostic.h"
#include "ir_generator.h"
#include "ir/statement.h"
#include "ir/type.h"
#include "ir/declaration.h"
#include "ir/compound_type.h"
#include "arch/x86_64.h"
#include "util/strings.h"

namespace toycc {
    std::shared_ptr<ir::Scope> generate_ir(const SourceMap& source_map, CParser::CompilationUnitContext* context) {
        IRGenerator generator(source_map, context);
        return generator.get();
    }

    void IRGenerator::add_primitive_type(std::string name, bool is_signed, ir::PrimitiveSemantic semantic, size_t size, size_t alignment) {
        ir::TypeIdentifier identifier = {.category = ir::TypeCategory::PRIMITIVE, .name = name};
        current_scope()->types[identifier] = std::make_shared<ir::PrimitiveType>(name, is_signed, semantic, size, alignment);
    }

    void IRGenerator::add_builtin_type(std::string name) {
        ir::TypeIdentifier identifier = {.category = ir::TypeCategory::BUILTIN, .name = name};
        CodeLocation location = {.filename = "<built-in>", .line = 1, .character = 1};
        std::shared_ptr<ir::Type> type_decl = std::make_shared<ir::Type> (identifier, location);
        current_scope()->types[identifier] = type_decl;

        // Built-in types will be identified as typedef names in the syntax, define them as such
        std::shared_ptr<ir::Declaration> typedef_decl = std::make_shared<ir::Declaration>
                (ir::Declaration {.name = name, .location = location, .storage = ir::StorageClass::TYPEDEF, .spec = {}});
        typedef_decl->spec.type = type_decl;
        current_scope()->typedefs[name] = typedef_decl;
    }

    void IRGenerator::init_global_scope() {
        // Initialize the global scope
        scope_stack.push_back(std::make_shared<ir::Scope>());
        current_scope()->function = nullptr;

        using namespace toycc::arch;
        ir::TypeIdentifier void_identifier = {.category = ir::TypeCategory::VOID, .name = "void"};
        current_scope()->types[void_identifier] = std::make_shared<ir::Type> (void_identifier, CodeLocation {.filename = "<built-in>", .line = 1, .character = 1});

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

    IRGenerator::IRGenerator(const SourceMap& source_map, CParser::CompilationUnitContext* context) : source_map(source_map) {
        init_global_scope();
        decode_compilation_unit(context);
    }

    std::shared_ptr<ir::Scope> IRGenerator::get() {
        if (scope_stack.size() != 1)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Found scopes other than the global scope in the scope stack after decoding ended");
        return scope_stack[0];
    }

    // ------------ Visitor

    void IRGenerator::decode_compilation_unit(CParser::CompilationUnitContext* context) {
        return decode_translation_unit(context->translationUnit());
    }

    void IRGenerator::decode_translation_unit(CParser::TranslationUnitContext* context) {
        for (CParser::ExternalDeclarationContext* declaration : context->externalDeclaration())
            decode_external_declaration(declaration);
    }

    void IRGenerator::decode_external_declaration(CParser::ExternalDeclarationContext* context) {
        if (context->declaration())
            decode_declaration(context->declaration());
        else if (context->functionDefinition())
            decode_function_definition(context->functionDefinition());
        else if (!context->Semi())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown external declaration `{}`", context->getText()), locate(context));
    }

    void IRGenerator::decode_function_definition(CParser::FunctionDefinitionContext* context) {
        if (context->declarationList())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Parameter declarations outside of the prototype are not supported", locate(context));

        ir::Declaration declaration;
        decode_declaration_specifiers(declaration, context->declarationSpecifiers()->declarationSpecifier());
        std::optional<std::string> name = decode_declarator(declaration.spec, context->declarator());

        if (!name.has_value())
            throw Diagnostic(DiagnosticLevel::ERROR, "Anonymous functions are not allowed", locate(context));
        declaration.name = name.value();

        if (!declaration.spec.is_function_type)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Function definition does not contain a function declaration", locate(context));

        std::shared_ptr<ir::Declaration> function_decl = declare(declaration);
        std::shared_ptr<ir::Scope> function_scope = create_function_scope(function_decl);
        add_statement(std::make_shared<ir::stmt::Function>(locate(context), function_scope, function_decl));
        decode_compound_statement(context->compoundStatement(), function_scope);
    }


    // ------------ Statements
    std::shared_ptr<ir::Scope> IRGenerator::decode_compound_statement(CParser::CompoundStatementContext* context) {
        std::shared_ptr<ir::Scope> scope = std::make_shared<ir::Scope>();
        scope->function = current_scope()->function;

        add_statement(std::make_shared<ir::stmt::Block>(locate(context), scope));
        decode_compound_statement(context, scope);

        return scope;
    }

    void IRGenerator::decode_compound_statement(CParser::CompoundStatementContext* context, std::shared_ptr<ir::Scope> scope) {
        ScopeFrame frame = in_scope(scope);

        if (context->blockItemList())
            decode_block_item_list(context->blockItemList());
    }

    void IRGenerator::decode_block_item_list(CParser::BlockItemListContext* context) {
        for (CParser::BlockItemContext* item : context->blockItem()) {
            if (item->declaration())
                decode_declaration(item->declaration());
            else if (item->statement())
                decode_statement(item->statement());
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown block item type `{}`", item->getText()), locate(item));
        }
    }

    void IRGenerator::decode_statement(CParser::StatementContext* context) {
        if (context->labeledStatement())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Labeled statements are not implemented", locate(context));
        else if (context->compoundStatement())
            decode_compound_statement(context->compoundStatement());
        else if (context->expressionStatement())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Expression statements are not implemented", locate(context));
        else if (context->selectionStatement())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Selection statements are not implemented", locate(context));
        else if (context->iterationStatement())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Iteration statements are not implemented", locate(context));
        else if (context->jumpStatement())
            decode_jump_statement(context->jumpStatement());
        else if (!context->logicalOrExpression().empty())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Inline assembly is not supported", locate(context));
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown statement `{}`", context->getText()), locate(context));
    }

    void IRGenerator::decode_jump_statement(CParser::JumpStatementContext* context) {
        if (context->Goto())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "`goto` statements are not implemented", locate(context));
        else if (context->Continue())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "`continue` statements are not implemented", locate(context));
        else if (context->Break())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "`break` statements are not implemented", locate(context));
        else if (context->Return())
            decode_return_statement(context);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown jump statement `{}`", context->getText()), locate(context));
    }

    void IRGenerator::decode_return_statement(CParser::JumpStatementContext* context) {
        std::shared_ptr<ir::Declaration> current_function = current_scope()->function;
        if (current_function.get() == nullptr)
            throw Diagnostic(DiagnosticLevel::ERROR, "Return statement outside of a function definition", locate(context));

        if (context->expression()) {
            ir::TypeSpecification return_type_spec = current_function->spec.return_type();
            std::shared_ptr<ir::Declaration> return_value = decode_expression(context->expression());
            if (return_value->spec != return_type_spec)
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Implicit casts are not implemented", locate(context));

            add_statement(std::make_shared<ir::stmt::Return>(locate(context), return_value));
        } else {
            if (!current_function->spec.is_void())
                throw Diagnostic(DiagnosticLevel::ERROR, "Return without a value within a function with a non-void return type", locate(context));
            add_statement(std::make_shared<ir::stmt::Return>(locate(context)));
        }
    }




    // ------------ Declarations

    // Decode a declaration and push it to the current scope
    void IRGenerator::decode_declaration(CParser::DeclarationContext* context) {
        if (context->staticAssertDeclaration())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Static assertions are not implemented", locate(context));

        ir::Declaration base_declaration;
        decode_declaration_specifiers(base_declaration, context->declarationSpecifiers()->declarationSpecifier());

        // First, only process the declarations
        std::vector<ir::Declaration> declarations;
        if (context->initDeclaratorList()) {
            for (CParser::InitDeclaratorContext* declarator : context->initDeclaratorList()->initDeclarator()) {
                ir::Declaration declaration = base_declaration;
                std::optional<std::string> name = decode_declarator(declaration.spec, declarator->declarator());
                if (name.has_value())
                    declaration.name = name.value();
                declarations.push_back(declaration);
            }
        } else {
            declarations.push_back(base_declaration);
        }

        std::vector<std::shared_ptr<ir::Declaration>> declared_variables;
        for (const ir::Declaration& declaration : declarations) {
            declaration.check(false);
            declared_variables.push_back(declare(declaration));
        }

        // Then the initializations
        if (context->initDeclaratorList()) {
            for (unsigned decl_index = 0; decl_index < declarations.size(); decl_index++) {
                std::shared_ptr<ir::Declaration> declaration = declared_variables[decl_index];
                CParser::InitDeclaratorContext* declarator = context->initDeclaratorList()->initDeclarator()[decl_index];
                if (declarator->initializer()) {
                    if (declaration->storage & ir::StorageClass::TYPEDEF)
                        throw Diagnostic(DiagnosticLevel::ERROR, "Initializers are not allowed in typedef declarations", locate(declarator->initializer()));

                    std::shared_ptr<ir::Declaration> initializer = decode_initializer(declarator->initializer());
                    emit_copy(declaration, initializer, locate(declarator->initializer()), true);
                }
            }
        }
    }

    void IRGenerator::decode_declaration_specifiers(ir::Declaration& declaration, std::vector<CParser::DeclarationSpecifierContext*> specifiers) {
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
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown declaration specifier `{}`", specifier->getText()), locate(specifier));
        }

        if (!declaration.storage)  // No specific keyword used -> auto storage duration
            declaration.storage = ir::StorageClass::AUTO;

        // In typedefs like `unsigned int my_type_t`, everything is in the type specifiers, which is otherwise invalid - split those
        const bool is_typedef = declaration.storage & ir::StorageClass::TYPEDEF;
        if (is_typedef && type_specifiers.back()->typedefName()) {
            declaration.name = type_specifiers.back()->typedefName()->Identifier()->getText();
            type_specifiers.pop_back();
        }

        ir::TypeSpecification initial_spec = resolve_type_specifier(type_specifiers, is_typedef);
        declaration.spec = initial_spec.merge(declaration.spec, locate(specifiers[0]));
    }

    ir::TypeSpecification IRGenerator::decode_specifier_qualifier_list(CParser::SpecifierQualifierListContext* context) {
        ir::TypeSpecification spec;

        std::vector<CParser::SpecifierQualifierContext*> specifiers = context->specifierQualifier();
        std::vector<CParser::TypeSpecifierContext*> type_specifiers;
        for (CParser::SpecifierQualifierContext* specifier : specifiers) {
            if (specifier->typeSpecifier())
                type_specifiers.push_back(specifier->typeSpecifier());
            else if (specifier->typeQualifier())
                spec.qualifiers |= decode_type_qualifier(specifier->typeQualifier());
            else
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown specifier qualifier `{}`", specifier->getText()), locate(context));
        }

        ir::TypeSpecification initial_spec = resolve_type_specifier(type_specifiers, false);
        return initial_spec.merge(spec, locate(context));
    }

    Flags<ir::StorageClass> IRGenerator::decode_storage_class(CParser::StorageClassSpecifierContext* context) {
        if      (context->Typedef())      return ir::StorageClass::TYPEDEF;
        else if (context->Extern())       return ir::StorageClass::EXTERN;
        else if (context->Static())       return ir::StorageClass::STATIC;
        else if (context->ThreadLocal())  return ir::StorageClass::THREAD_LOCAL;
        else if (context->Auto())         return ir::StorageClass::AUTO;
        else if (context->Register())     return ir::StorageClass::REGISTER;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown storage class `{}`", context->getText()), locate(context));
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
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown type qualifier `{}`", context->getText()), locate(context));
    }

    Flags<ir::FunctionSpecifier> IRGenerator::decode_function_specifier(CParser::FunctionSpecifierContext* context) {
        if      (context->Inline())    return ir::FunctionSpecifier::INLINE;
        else if (context->Noreturn())  return ir::FunctionSpecifier::NORETURN;
        else if (context->gccAttributeSpecifier()) return {};  // Skip
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown function specifier `{}`", context->getText()), locate(context));
    }

    size_t IRGenerator::resolve_alignment_specifier(CParser::AlignmentSpecifierContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Alignment specifiers are not implemented", locate(context));
    }

    ir::TypeSpecification IRGenerator::resolve_type_specifier(std::vector<CParser::TypeSpecifierContext*> type_specifiers, bool is_typedef) {
        const CodeLocation location = locate(type_specifiers[0]);

        ir::TypeIdentifier identifier = decode_type_specifier(type_specifiers);
        std::optional<ir::TypeSpecification> spec = resolve_type_without_error(identifier);

        // Declare incomplete types in typedefs
        if (!spec.has_value() && is_typedef && (identifier.category == ir::TypeCategory::STRUCT || identifier.category == ir::TypeCategory::UNION || identifier.category == ir::TypeCategory::ENUM)) {
            current_scope()->types[identifier] = std::make_shared<ir::CompoundType>(identifier, location);
            spec = resolve_type(identifier, location);
        }

        return spec.value();
    }

    // Decode a type specifier, push eventual anonymous struct/enum/union declarations to the current scope and return the type identifier
    // Don't check whether non-primitive named types exists
    ir::TypeIdentifier IRGenerator::decode_type_specifier(std::vector<CParser::TypeSpecifierContext*> type_specifiers) {
        if (type_specifiers.empty())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Empty type specifier list");

        const CodeLocation base_location = locate(type_specifiers[0]);

        std::optional<std::string> sign_token;
        std::multiset<std::string> primitive_type_tokens;
        std::optional<ir::TypeIdentifier> identifier;

        for (CParser::TypeSpecifierContext* specifier : type_specifiers) {
            const std::string token = specifier->getText();
            const CodeLocation location = locate(specifier);

            if (specifier->Void() || specifier->Char() || specifier->Bool() || specifier->Float() || specifier->Short() || specifier->Int() || specifier->Double() || specifier->Long()) {
                primitive_type_tokens.insert(token);
            } else if (specifier->Signed() || specifier->Unsigned()) {
                if (sign_token.has_value())
                    throw Diagnostic(DiagnosticLevel::ERROR, "Declaration can't have more than one sign specifier", location);
                sign_token = token;
            } else if (specifier->typedefName()) {
                if (identifier.has_value())
                    throw Diagnostic(DiagnosticLevel::ERROR, "Non-primitive types can't have more than one identifier", location);
                identifier = ir::TypeIdentifier {.category = ir::TypeCategory::TYPEDEF, .name = token};
            }
            else if (specifier->Complex())                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Complex types are unsupported",                locate(specifier));
            else if (specifier->atomicTypeSpecifier())    throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Atomic type specifiers are unsupported",       locate(specifier));
            else if (specifier->structOrUnionSpecifier()) {
                if (identifier.has_value())
                    throw Diagnostic(DiagnosticLevel::ERROR, "Non-primitive types can't have more than one identifier", location);
                identifier = decode_struct_or_union_specifier(specifier->structOrUnionSpecifier());
            }
            else if (specifier->enumSpecifier())          throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Enums are unsupported",                        locate(specifier));
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown type specifier `{}`", specifier->getText()), locate(specifier));
        }

        if (!identifier.has_value() && !sign_token.has_value() && primitive_type_tokens.empty())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "No valid type specifier found", base_location);

        if (identifier.has_value()) {
            if (sign_token.has_value() || !primitive_type_tokens.empty())
                throw Diagnostic(DiagnosticLevel::ERROR, "Non-primitive type name can't have primitive type specifiers", base_location);
            return identifier.value();
        }

        // ---- Now resolve primitive types. From now on `typedef_token` is empty

        // Void and bool are always alone
        if (primitive_type_tokens.contains("_Bool") || primitive_type_tokens.contains("void")) {
            if (primitive_type_tokens.size() > 1 || sign_token.has_value())
                throw Diagnostic(DiagnosticLevel::ERROR, "Primitive types _Bool and void can't have other type specifiers", base_location);
            return {ir::TypeCategory::PRIMITIVE, *primitive_type_tokens.begin()};
        }

        // Floating-point types
        if (primitive_type_tokens.contains("float") || primitive_type_tokens.contains("double")) {
            if (sign_token.has_value())
                throw Diagnostic(DiagnosticLevel::ERROR, "Floating-point type can't have a sign specifier", base_location);

            if (primitive_type_tokens.contains("float")) {
                if (primitive_type_tokens.size() > 1)
                    throw Diagnostic(DiagnosticLevel::ERROR, "Primitive type `float` can't have any other type specifiers", base_location);
                return {ir::TypeCategory::PRIMITIVE, "float"};
            }

            // From now on, only double remains
            if (primitive_type_tokens.contains("long") && primitive_type_tokens.size() == 2)
                return {ir::TypeCategory::PRIMITIVE, "long double"};
            else if (primitive_type_tokens.size() == 1)
                return {ir::TypeCategory::PRIMITIVE, "double"};
            else
                throw Diagnostic(DiagnosticLevel::ERROR, "Primitive type `double` can't have any type specifiers other than `long`", base_location);
        }

        // From now on, only integer types remain and sign_token has a value
        // signed / unsigned without a type -> implied int; type without a sign qualifier -> implied signed
        if (sign_token.has_value() && primitive_type_tokens.empty())
            primitive_type_tokens.insert("int");
        else if (!sign_token.has_value() && !primitive_type_tokens.empty())
            sign_token = "signed";

        if (primitive_type_tokens.count("int") > 1)
            throw Diagnostic(DiagnosticLevel::ERROR, "Primitive type can't specify `int` more than once", base_location);

        if (primitive_type_tokens.contains("char")) {
            if (primitive_type_tokens.size() > 1)
                throw Diagnostic(DiagnosticLevel::ERROR, "Primitive type `char` can't have type specifiers other than `signed`/`unsigned`", base_location);
            return {ir::TypeCategory::PRIMITIVE, std::format("{} char", sign_token.value())};
        }

        // From now on, only short, int and long remain
        if (primitive_type_tokens.contains("short")) {
            if (primitive_type_tokens.contains("long"))
                throw Diagnostic(DiagnosticLevel::ERROR, "Primitive type `short` can't also be `long`", base_location);
            return {ir::TypeCategory::PRIMITIVE, std::format("{} short int", sign_token.value())};
        }

        // From now on, only long and int remain
        const size_t nof_longs = primitive_type_tokens.count("long");
        switch (nof_longs) {
            case 0:  return {ir::TypeCategory::PRIMITIVE, std::format("{} int", sign_token.value())};
            case 1:  return {ir::TypeCategory::PRIMITIVE, std::format("{} long int", sign_token.value())};
            case 2:  return {ir::TypeCategory::PRIMITIVE, std::format("{} long long int", sign_token.value())};
            default: throw Diagnostic(DiagnosticLevel::ERROR, "Primitive type specifier `long` can't be given more than twice", base_location);
        }
    }

    // Decode a struct or union specifier, push struct/enum/union definitions to the current scope and return the type identifier
    // Don't check whether named struct / unions exists
    ir::TypeIdentifier IRGenerator::decode_struct_or_union_specifier(CParser::StructOrUnionSpecifierContext* context) {
        const CodeLocation location = locate(context);
        ir::TypeIdentifier identifier;

        if (context->Identifier()) identifier.name = context->Identifier()->getText();
        else                       identifier.name = anonymous_identifier();

        if      (context->structOrUnion()->Struct())  identifier.category = ir::TypeCategory::STRUCT;
        else if (context->structOrUnion()->Union())   identifier.category = ir::TypeCategory::UNION;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown struct or union keyword `{}`", context->structOrUnion()->getText()), location);

        if (context->structDeclarationList()) {
            std::shared_ptr<ir::CompoundType> definition = std::make_shared<ir::CompoundType>(identifier, location);
            current_scope()->types[identifier] = definition;  // Push the incomplete type to the current scope to allow struct members to use it

            for (CParser::StructDeclarationContext* declaration : context->structDeclarationList()->structDeclaration())
                definition->members.append_range(decode_struct_declaration(declaration));

            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Complete structure declaration are not implemented", location);
        }

        return identifier;
    }


    std::vector<ir::StructMember> IRGenerator::decode_struct_declaration(CParser::StructDeclarationContext* context) {
        const CodeLocation location = locate(context);

        if (context->staticAssertDeclaration())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Static assertions are not supported", location);

        ir::TypeSpecification base_spec = decode_specifier_qualifier_list(context->specifierQualifierList());
        if (!context->structDeclaratorList()) {
            ir::StructMember member = {.name = anonymous_identifier(), .location = location, .spec = base_spec};
            return {member};
        }

        std::vector<ir::StructMember> members;
        for (CParser::StructDeclaratorContext* declarator : context->structDeclaratorList()->structDeclarator()) {
            const CodeLocation declarator_location = locate(declarator);
            if (declarator->constantExpression())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Bitfields are not implemented", declarator_location);
            else if (!declarator->declarator())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Anonymous bitfields are not implemented", declarator_location);

            ir::TypeSpecification member_spec = base_spec;
            std::optional<std::string> name = decode_declarator(member_spec, declarator->declarator());
            std::string member_name = name.value_or(anonymous_identifier());
            members.push_back(ir::StructMember {.name = member_name, .location = declarator_location, .spec = member_spec});
        }

        return members;
    }

    std::optional<std::string> IRGenerator::decode_declarator(ir::TypeSpecification& spec, CParser::DeclaratorContext* context) {
        if (!context->gccDeclaratorExtension().empty())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GCC declarator extensions are not supported", locate(context));

        if (context->pointer())
            spec.pointer_spec = decode_pointer_spec(context->pointer());

        return decode_direct_declarator(spec, context->directDeclarator());
    }

    std::optional<std::string> IRGenerator::decode_direct_declarator(ir::TypeSpecification& spec, CParser::DirectDeclaratorContext* context) {
        const CodeLocation location = locate(context);

        if (context->DigitSequence()) {  // Bitfield alternative
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Bitfields are not implemented", location);
        } else if (context->vcSpecificModifier()) {  // VC-specific alternatives
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "VC declarator extensions are not supported", location);
        } else if (context->declarator()) {  // Parenthesized alternative
            return decode_declarator(spec, context->declarator());
        } else if (context->Identifier()) {  // Identifier alternative
            return context->Identifier()->getText();
        } else if (context->directDeclarator() && context->LeftParen() && context->RightParen()) {  // Function alternatives
            return decode_function_direct_declarator(spec, context);
        } else if (context->typeQualifierList() || context->Static() || context->Star()) {  // Array alternatives with inner declarations
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "This type of array declarator is not implemented", location);
        } else if (context->assignmentExpression()) {  // Array alternative
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Array declarators are not implemented", location);
        } else {
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown declarator type", location);
        }

        return {};
    }

    std::optional<std::string> IRGenerator::decode_function_direct_declarator(ir::TypeSpecification& spec, CParser::DirectDeclaratorContext* context) {
        spec.is_function_type = true;
        std::optional<std::string> name = decode_direct_declarator(spec, context->directDeclarator());

        if (context->identifierList())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Function declarators with untyped parameters are not supported", locate(context));
        else if (context->parameterTypeList())
            spec.parameters.append_range(decode_parameter_type_list(context->parameterTypeList()));
        // Otherwise, no parameters

        return name;
    }

    std::vector<ir::Declaration> IRGenerator::decode_parameter_type_list(CParser::ParameterTypeListContext* context) {
        if (context->Ellipsis())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Variadic functions are not implemented", locate(context));

        return decode_parameter_list(context->parameterList());
    }

    std::vector<ir::Declaration> IRGenerator::decode_parameter_list(CParser::ParameterListContext* context) {
        std::vector<ir::Declaration> parameters;
        for (CParser::ParameterDeclarationContext* parameter : context->parameterDeclaration()) {
            ir::Declaration declaration = decode_parameter_declaration(parameter);
            if (declaration.spec.is_void()) {
                if (!declaration.name.empty() || context->parameterDeclaration().size() > 1)
                    throw Diagnostic(DiagnosticLevel::ERROR, "Invalid void parameter declaration", locate(parameter));
            } else {
                parameters.push_back(declaration);  // Only add non-void parameters
            }
        }

        return parameters;
    }

    ir::Declaration IRGenerator::decode_parameter_declaration(CParser::ParameterDeclarationContext* context) {
        ir::Declaration parameter;

        if (context->declarationSpecifiers())
            decode_declaration_specifiers(parameter, context->declarationSpecifiers()->declarationSpecifier());
        else if (context->declarationSpecifiers2())
            decode_declaration_specifiers(parameter, context->declarationSpecifiers2()->declarationSpecifier());
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "No declaration specifiers found in parameter declaration", locate(context));

        std::optional<std::string> name;
        if (context->declarator())
            name = decode_declarator(parameter.spec, context->declarator());
        else if (context->abstractDeclarator())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Abstract parameter declarators are not implemented", locate(context));
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "No declarator found in parameter declaration", locate(context));

        if (name.has_value())
            parameter.name = name.value();

        parameter.storage |= ir::StorageClass::PARAMETER;
        return parameter;
    }

    void IRGenerator::decode_initializer(ir::Declaration const&, CParser::InitializerContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Initializers are not implemented", locate(context));
    }

    std::vector<Flags<ir::TypeQualifier>> IRGenerator::decode_pointer_spec(CParser::PointerContext* context) {
        std::vector<Flags<ir::TypeQualifier>> pointer_spec(context->pointerLevel().size());
        for (CParser::PointerLevelContext* level : context->pointerLevel()) {
            if (level->Caret())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Carets in pointer specification are not supported", locate(level));

            if (level->typeQualifierList())
                pointer_spec.push_back(decode_type_qualifier_list(level->typeQualifierList()));
            else
                pointer_spec.emplace_back();
        }

        return pointer_spec;
    }


    // ------------ Expressions

    std::shared_ptr<ir::Declaration> IRGenerator::decode_initializer(CParser::InitializerContext* context) {
        if (context->initializerList())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Initializer lists are not implemented", locate(context));
        else if (context->assignmentExpression())
            return decode_assignment_expression(context->assignmentExpression());
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown initializer `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_expression(CParser::ExpressionContext* context) {
        std::shared_ptr<ir::Declaration> result;
        for (CParser::AssignmentExpressionContext* expression : context->assignmentExpression())
            result = decode_assignment_expression(expression);
        return result;  // In a comma-separated list of expressions, return the last one
    }


    std::shared_ptr<ir::Declaration> IRGenerator::decode_assignment_expression(CParser::AssignmentExpressionContext* context) {
        if (context->conditionalExpression())
            return decode_conditional_expression(context->conditionalExpression());

        if (context->DigitSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Digit sequences are not supported as assignment expressions", locate(context));

        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Assignment expressions are not implemented", locate(context));
        //const std::optional<ir::stmt::BinaryOperator> op = decode_assignment_operator(context->assignmentOperator());
        //std::shared_ptr<ir::Declaration> right_operand = decode_assignment_expression(context->assignmentExpression());
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_conditional_expression(CParser::ConditionalExpressionContext* context) {
        std::shared_ptr<ir::Declaration> predicate = decode_logical_or_expression(context->logicalOrExpression());

        if (!context->Question())
            return predicate;

        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Conditional expressions are not implemented", locate(context));
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_logical_or_expression(CParser::LogicalOrExpressionContext* context) {
        std::shared_ptr<ir::Declaration> result = decode_logical_and_expression(context->logicalAndExpression()[0]);
        if (context->logicalAndExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Logical OR expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_logical_and_expression(CParser::LogicalAndExpressionContext* context) {
        std::shared_ptr<ir::Declaration> result = decode_inclusive_or_expression(context->inclusiveOrExpression()[0]);
        if (context->inclusiveOrExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Logical AND expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_inclusive_or_expression(CParser::InclusiveOrExpressionContext* context) {
        std::shared_ptr<ir::Declaration> result = decode_exclusive_or_expression(context->exclusiveOrExpression()[0]);
        if (context->exclusiveOrExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Inclusive OR expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_exclusive_or_expression(CParser::ExclusiveOrExpressionContext* context) {
        std::shared_ptr<ir::Declaration> result = decode_and_expression(context->andExpression()[0]);
        if (context->andExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Exclusive OR expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_and_expression(CParser::AndExpressionContext* context) {
        std::shared_ptr<ir::Declaration> result = decode_equality_expression(context->equalityExpression()[0]);
        if (context->equalityExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "AND expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_equality_expression(CParser::EqualityExpressionContext* context) {
        std::shared_ptr<ir::Declaration> result = decode_relational_expression(context->relationalExpression()[0]);
        if (context->relationalExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Equality expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_relational_expression(CParser::RelationalExpressionContext* context) {
        std::shared_ptr<ir::Declaration> result = decode_shift_expression(context->shiftExpression()[0]);
        if (context->shiftExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Relational expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_shift_expression(CParser::ShiftExpressionContext* context) {
        std::shared_ptr<ir::Declaration> result = decode_additive_expression(context->additiveExpression()[0]);
        if (context->additiveExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Shift expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_additive_expression(CParser::AdditiveExpressionContext* context) {
        std::shared_ptr<ir::Declaration> result = decode_multiplicative_expression(context->multiplicativeExpression()[0]);
        if (context->multiplicativeExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Additive expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_multiplicative_expression(CParser::MultiplicativeExpressionContext* context) {
        std::shared_ptr<ir::Declaration> result = decode_cast_expression(context->castExpression()[0]);
        if (context->castExpression().size() > 1)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Multiplicative expressions are not implemented", locate(context));
        return result;
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_cast_expression(CParser::CastExpressionContext* context) {
        if (context->DigitSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Digit sequences as cast expressions are not implemented", locate(context));
        else if (context->typeName())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Cast expressions are not implemented");
        else if (context->unaryExpression())
            return decode_unary_expression(context->unaryExpression());
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown cast expression `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_unary_expression(CParser::UnaryExpressionContext* context) {
        if (!context->PlusPlus().empty() || !context->MinusMinus().empty() || !context->Sizeof().empty() || context->Alignof() || context->AndAnd() || context->unaryOperator() || context->castExpression())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unary expressions are not implemented", locate(context));

        return decode_postfix_expression(context->postfixExpression());
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_postfix_expression(CParser::PostfixExpressionContext* context) {
        if (!context->primaryExpression() || !context->LeftParen().empty() || !context->LeftBracket().empty() || !context->Dot().empty() || !context->Arrow().empty() || !context->PlusPlus().empty() || !context->MinusMinus().empty())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Postfix expressions are not implemented", locate(context));

        return decode_primary_expression(context->primaryExpression());
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_primary_expression(CParser::PrimaryExpressionContext* context) {
        if (context->Identifier())
            return resolve(context->Identifier()->getText(), locate(context->Identifier()));
        else if (context->Constant())
            return decode_constant(context->Constant());
        else if (!context->StringLiteral().empty())
            return decode_string_literal(context->StringLiteral());
        else if (context->LeftParen() && context->expression() && context->RightParen())
            return decode_expression(context->expression());
        else if (context->genericSelection())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Generics are not implemented", locate(context));
        else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Unknown primary expression `{}`", context->getText()), locate(context));
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_constant(antlr4::tree::TerminalNode* terminal) {
        const std::string text = terminal->getText();
        if (text.starts_with("'") || text.starts_with("L'") || text.starts_with("u'") || text.starts_with("U'"))
            return decode_character_constant(terminal);
        else if (text.contains("."))
            return decode_floating_constant(terminal);
        else
            return decode_integer_constant(terminal);
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_character_constant(antlr4::tree::TerminalNode* terminal) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Character constants are not implemented", locate(terminal));
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_floating_constant(antlr4::tree::TerminalNode* terminal) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Floating constants are not implemented", locate(terminal));
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_integer_constant(antlr4::tree::TerminalNode* terminal) {
        const std::string text = terminal->getText();
        if (text.at(0) == '0') {
            if (text.length() == 1)
                return decode_decimal_constant(terminal);  // Literal 0
            else if (text.at(1) == 'x' || text.at(1) == 'X')
                return decode_hexadecimal_constant(terminal);
            else if (text.at(1) == 'b' || text.at(1) == 'B')
                return decode_binary_constant(terminal);
            else
                return decode_octal_constant(terminal);
        } else {
            return decode_decimal_constant(terminal);
        }
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_decimal_constant(antlr4::tree::TerminalNode* terminal) {
        const std::string text = terminal->getText();
        const size_t suffix_position = text.find_first_not_of("0123456789");

        size_t decimal_end;
        const size_t value = std::stoull(text, &decimal_end);
        if (decimal_end != text.length() && decimal_end != suffix_position)
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Found garbage in decimal constant `{}`", text), locate(terminal));

        std::string suffix;
        if (suffix_position != std::string::npos)
            suffix = text.substr(suffix_position);
        return declare_integer_constant(value, suffix, locate(terminal));
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_hexadecimal_constant(antlr4::tree::TerminalNode* terminal) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Hexadecimal constants are not implemented", locate(terminal));
    }

    std::shared_ptr<ir::Declaration> IRGenerator::decode_binary_constant(antlr4::tree::TerminalNode* terminal) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Binary constants are not implemented", locate(terminal));
    }

    std::shared_ptr<ir::Declaration>IRGenerator::decode_octal_constant(antlr4::tree::TerminalNode* terminal) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Octal constants are not implemented", locate(terminal));
    }


    std::shared_ptr<ir::Declaration> IRGenerator::decode_string_literal(std::vector<antlr4::tree::TerminalNode*> terminals) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "String literals are not implemented", locate(terminals[0]));
    }


    std::shared_ptr<ir::Declaration> IRGenerator::declare_integer_constant(size_t base_value, std::string suffix, CodeLocation location) {
        to_lower_inplace(suffix);
        ir::TypeIdentifier type_identifier = {.category = ir::TypeCategory::PRIMITIVE, .name = ""};
        ir::stmt::LoadConst::Constant value;

        if      (suffix.empty())                     { value = static_cast<int>               (base_value); type_identifier.name = "signed int";             }
        else if (suffix == "u")                      { value = static_cast<unsigned int>      (base_value); type_identifier.name = "unsigned int";           }
        else if (suffix == "l")                      { value = static_cast<long>              (base_value); type_identifier.name = "signed long int";        }
        else if (suffix == "ll")                     { value = static_cast<unsigned long>     (base_value); type_identifier.name = "signed long long int";   }
        else if (suffix == "ul" || suffix == "lu")   { value = static_cast<long long>         (base_value); type_identifier.name = "unsigned long int";      }
        else if (suffix == "ull" || suffix == "llu") { value = static_cast<unsigned long long>(base_value); type_identifier.name = "unsigned long long int"; }
        else throw Diagnostic(DiagnosticLevel::ERROR, std::format("Unknown integer literal suffix `{}`", suffix), location);

        ir::TypeSpecification spec = resolve_type(type_identifier, location);
        std::shared_ptr<ir::Declaration> declaration = declare(ir::Declaration
                {.name = anonymous_identifier(), .location = location, .storage = ir::StorageClass::AUTO | ir::StorageClass::TEMPORARY, .spec = spec});

        add_statement(std::make_shared<ir::stmt::LoadConst> (location, declaration, value));
        return declaration;
    }


    std::optional<ir::stmt::BinaryOperator> IRGenerator::decode_assignment_operator(CParser::AssignmentOperatorContext* context) {
        if      (context->Assign())            return {};
        else if (context->StarAssign())        return ir::stmt::BinaryOperator::MUL;
        else if (context->DivAssign())         return ir::stmt::BinaryOperator::DIV;
        else if (context->ModAssign())         return ir::stmt::BinaryOperator::MOD;
        else if (context->PlusAssign())        return ir::stmt::BinaryOperator::PLUS;
        else if (context->MinusAssign())       return ir::stmt::BinaryOperator::MINUS;
        else if (context->LeftShiftAssign())   return ir::stmt::BinaryOperator::LSHIFT;
        else if (context->RightShiftAssign())  return ir::stmt::BinaryOperator::RSHIFT;
        else if (context->AndAssign())         return ir::stmt::BinaryOperator::BITWISE_AND;
        else if (context->XorAssign())         return ir::stmt::BinaryOperator::BITWISE_XOR;
        else if (context->OrAssign())          return ir::stmt::BinaryOperator::BITWISE_OR;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown assignment operator {}", context->getText()), locate(context));
    }


    // ------------ IR emission common functions
    void IRGenerator::emit_copy(std::shared_ptr<ir::Declaration> destination, std::shared_ptr<ir::Declaration> source, CodeLocation location, bool initialize) {
        if (source->spec != destination->spec)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Implicit casts are not implemented", location);

        if (!initialize && (destination->spec.qualifiers & ir::TypeQualifier::CONST))
            throw Diagnostic(DiagnosticLevel::ERROR, "Attempted to assign a value to a constant after initialization", location);

        add_statement(std::make_shared<ir::stmt::Copy>(location, destination, source));
    }


    // ------------ Utilities
    std::shared_ptr<ir::Scope> IRGenerator::current_scope() {
        return scope_stack.back();
    }

    std::optional<CodeLocation> IRGenerator::locate_name(std::string name, bool current_scope_only) {
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

    CodeLocation IRGenerator::locate(antlr4::ParserRuleContext* context) const {
        antlr4::Token* start_token = context->getStart();
        LinePosition line = source_map.at(start_token->getLine());
        return {.filename = line.filename, .line = line.line, .character = start_token->getCharPositionInLine()};
    }

    CodeLocation IRGenerator::locate(antlr4::tree::TerminalNode* token) const {
        LinePosition line = source_map.at(token->getSymbol()->getLine());
        return {.filename = line.filename, .line = line.line, .character = token->getSymbol()->getCharPositionInLine()};
    }

    std::string IRGenerator::anonymous_identifier() {
        return std::format("<anonymous_{}>", unique_id++);
    }

    std::shared_ptr<ir::Scope> IRGenerator::create_function_scope(std::shared_ptr<ir::Declaration> declaration) {
        std::shared_ptr<ir::Scope> scope = std::make_shared<ir::Scope>();
        scope->function = declaration;
        for (const ir::Declaration& parameter : declaration->spec.parameters)
            if (!parameter.name.empty())
                scope->locals[parameter.name] = std::make_shared<ir::Declaration>(parameter);

        return scope;
    }

    std::shared_ptr<ir::Declaration> IRGenerator::declare(ir::Declaration declaration) {
        std::optional<CodeLocation> existing_location = locate_name(declaration.name);
        if (existing_location.has_value())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Name {} was already declared", declaration.name), declaration.location)
                   .add_note(DiagnosticLevel::NOTE, "Previously declared here", existing_location.value());

        if (declaration.storage & ir::StorageClass::TYPEDEF)
            return (current_scope()->typedefs[declaration.name] = std::make_shared<ir::Declaration>(declaration));
        else
            return (current_scope()->locals[declaration.name] = std::make_shared<ir::Declaration>(declaration));
    }

    std::shared_ptr<ir::Statement> IRGenerator::add_statement(std::shared_ptr<ir::Statement> statement) {
        current_scope()->statements.push_back(statement);
        return current_scope()->statements.back();
    }

    std::shared_ptr<ir::Declaration> IRGenerator::resolve_without_error(std::string name) {
        for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); it++) {
            std::shared_ptr<ir::Scope> scope = *it;
            if (scope->locals.contains(name))
                return scope->locals.at(name);
        }

        return nullptr;
    }

    std::shared_ptr<ir::Declaration> IRGenerator::resolve(std::string name, CodeLocation location) {
        std::shared_ptr<ir::Declaration> declaration = resolve_without_error(name);

        if (declaration.get() == nullptr)
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Name `{}` was not declared", name), location);
        return declaration;
    }

    std::optional<ir::TypeSpecification> IRGenerator::resolve_type_without_error(ir::TypeIdentifier identifier) {
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

    ir::TypeSpecification IRGenerator::resolve_type(ir::TypeIdentifier identifier, CodeLocation location) {
        std::optional<ir::TypeSpecification> spec = resolve_type_without_error(identifier);
        if (!spec.has_value())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Type `{}` was not declared", identifier.text()), location);
        return spec.value();
    }



    // ------------ ScopeFrame
    IRGenerator::ScopeFrame::ScopeFrame(std::deque<std::shared_ptr<ir::Scope>>& scope_stack, std::shared_ptr<ir::Scope> scope) : scope_stack(scope_stack) {
        scope_stack.push_back(scope);
    }

    IRGenerator::ScopeFrame::~ScopeFrame() {
        scope_stack.pop_back();
    }

    IRGenerator::ScopeFrame IRGenerator::in_scope(std::shared_ptr<ir::Scope> scope) {
        return {scope_stack, scope};
    }
}
