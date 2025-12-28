#include "diagnostic.h"
#include "ir/generator.h"

namespace toycc::ir {
    // Decode a declaration and push it to the current scope
    void Generator::decode_declaration(CParser::DeclarationContext* context) {
        if (context->staticAssertDeclaration())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Static assertions are not implemented", locate(context));
        if (context->attributeDeclaration())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Attribute declarations are not implemented", locate(context));

        Declaration base_declaration;
        decode_declaration_specifiers(base_declaration, context->declarationSpecifiers());

        // First, only process the declarations
        std::vector<Declaration> declarations;
        if (context->initDeclaratorList()) {
            for (CParser::InitDeclaratorContext* declarator : context->initDeclaratorList()->initDeclarator()) {
                Declaration declaration = base_declaration;
                std::optional<std::string> name = decode_declarator(declaration.spec, declarator->declarator());
                if (name.has_value())
                    declaration.name = name.value();
                declarations.push_back(declaration);
            }
        } else {
            declarations.push_back(base_declaration);
        }

        std::vector<std::shared_ptr<Declaration>> declared_variables;
        for (const Declaration& declaration : declarations) {
            declaration.check(false);
            declared_variables.push_back(declare(declaration));
        }

        // Then the initializations
        if (context->initDeclaratorList()) {
            for (unsigned decl_index = 0; decl_index < declarations.size(); decl_index++) {
                std::shared_ptr<Declaration> declaration = declared_variables[decl_index];
                CParser::InitDeclaratorContext* declarator = context->initDeclaratorList()->initDeclarator()[decl_index];
                if (declarator->initializer()) {
                    if (declaration->storage & StorageClass::TYPEDEF)
                        throw Diagnostic(DiagnosticLevel::ERROR, "Initializers are not allowed in typedef declarations", locate(declarator->initializer()));

                    std::shared_ptr<Declaration> initializer = decode_initializer(declarator->initializer());
                    emit_copy(declaration, initializer, locate(declarator->initializer()), true);
                }
            }
        }
    }

    void Generator::decode_declaration_specifiers(Declaration& declaration, CParser::DeclarationSpecifiersContext* specifiers) {
        std::vector<CParser::TypeSpecifierContext*> type_specifiers;
        for (CParser::DeclarationSpecifierContext* specifier : specifiers->declarationSpecifier()) {
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
            declaration.storage = StorageClass::AUTO;

        const bool is_typedef = declaration.storage & StorageClass::TYPEDEF;
        TypeSpecification initial_spec = resolve_type_specifier(type_specifiers, is_typedef);
        declaration.spec = initial_spec.merge(declaration.spec, locate(specifiers));
    }

    TypeSpecification Generator::decode_specifier_qualifier_list(CParser::SpecifierQualifierListContext* context) {
        if (context->gnuAttributes())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GNU attributes are not supported", locate(context));
        if (context->attributeSpecifierSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Attribute specifiers are not supported", locate(context));
        TypeSpecification spec;

        std::vector<CParser::TypeSpecifierQualifierContext*> specifiers = context->typeSpecifierQualifier();
        std::vector<CParser::TypeSpecifierContext*> type_specifiers;
        for (CParser::TypeSpecifierQualifierContext* specifier : specifiers) {
            if (specifier->typeSpecifier())
                type_specifiers.push_back(specifier->typeSpecifier());
            else if (specifier->typeQualifier())
                spec.qualifiers |= decode_type_qualifier(specifier->typeQualifier());
            else if (specifier->alignmentSpecifier())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Alignment specifiers are not supported", locate(context));
            else
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown specifier qualifier `{}`", specifier->getText()), locate(context));
        }

        TypeSpecification initial_spec = resolve_type_specifier(type_specifiers, false);
        return initial_spec.merge(spec, locate(context));
    }

    Flags<StorageClass> Generator::decode_storage_class(CParser::StorageClassSpecifierContext* context) {
        if      (context->Typedef())      return StorageClass::TYPEDEF;
        else if (context->Extern())       return StorageClass::EXTERN;
        else if (context->Static())       return StorageClass::STATIC;
        else if (context->ThreadLocal())  return StorageClass::THREAD_LOCAL;
        else if (context->Auto())         return StorageClass::AUTO;
        else if (context->Register())     return StorageClass::REGISTER;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown storage class `{}`", context->getText()), locate(context));
    }

    Flags<TypeQualifier> Generator::decode_type_qualifier_list(CParser::TypeQualifierListContext* context) {
        Flags<TypeQualifier> qualifiers;
        for (CParser::TypeQualifierContext* qualifier : context->typeQualifier())
            qualifiers |= decode_type_qualifier(qualifier);
        return qualifiers;
    }

    Flags<TypeQualifier> Generator::decode_type_qualifier(CParser::TypeQualifierContext* context) {
        if      (context->Const())     return TypeQualifier::CONST;
        else if (context->Restrict())  return TypeQualifier::RESTRICT;
        else if (context->Volatile())  return TypeQualifier::VOLATILE;
        else if (context->Atomic())    return TypeQualifier::ATOMIC;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown type qualifier `{}`", context->getText()), locate(context));
    }

    Flags<FunctionSpecifier> Generator::decode_function_specifier(CParser::FunctionSpecifierContext* context) {
        if      (context->Inline() || context->KW__inline__()) return FunctionSpecifier::INLINE;
        else if (context->Noreturn())                          return FunctionSpecifier::NORETURN;
        else if (context->KW__stdcall())                       return FunctionSpecifier::STDCALL;
        else if (context->gnuAttribute()) throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GNU attributes are not supported", locate(context));
        else if (context->KW__declspec()) throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Declspec specifiers are not supported", locate(context));
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown function specifier `{}`", context->getText()), locate(context));
    }

    size_t Generator::resolve_alignment_specifier(CParser::AlignmentSpecifierContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Alignment specifiers are not implemented", locate(context));
    }

    TypeSpecification Generator::resolve_type_specifier(std::vector<CParser::TypeSpecifierContext*> type_specifiers, bool is_typedef) {
        const CodeLocation location = locate(type_specifiers[0]);

        TypeIdentifier identifier = decode_type_specifiers(type_specifiers);
        std::optional<TypeSpecification> spec = resolve_type_without_error(identifier);

        // Declare incomplete types in typedefs
        if (!spec.has_value() && is_typedef && (identifier.category == TypeCategory::STRUCT || identifier.category == TypeCategory::UNION || identifier.category == TypeCategory::ENUM)) {
            current_scope()->add_type(std::make_shared<CompoundType>(identifier, location));
            spec = resolve_type(identifier, location);
        }

        if (!spec.has_value())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Type `{}` was not declared", identifier.text()), location);

        return spec.value();
    }

    // Decode a type specifier, push eventual anonymous struct/enum/union declarations to the current scope and return the type identifier
    // Don't check whether non-primitive named types exists
    TypeIdentifier Generator::decode_type_specifiers(std::vector<CParser::TypeSpecifierContext*> type_specifiers) {
        if (type_specifiers.empty())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Empty type specifier list");

        const CodeLocation base_location = locate(type_specifiers[0]);

        std::optional<std::string> sign_token;
        std::multiset<std::string> primitive_type_tokens;
        std::optional<TypeIdentifier> identifier;

        for (CParser::TypeSpecifierContext* specifier : type_specifiers) {
            const std::string token = specifier->getText();
            const CodeLocation location = locate(specifier);

            if (specifier->Void()) {
                if (identifier.has_value())
                    throw Diagnostic(DiagnosticLevel::ERROR, "Void types can't have more than one identifier", location);
                identifier = TypeIdentifier {.category = TypeCategory::VOID, .name = token};
            } else if (specifier->Char() || specifier->Short() || specifier->Int() || specifier->Long() || specifier->Float() || specifier->Double() || specifier->Bool()) {
                primitive_type_tokens.insert(token);
            } else if (specifier->Signed() || specifier->Unsigned()) {
                if (sign_token.has_value())
                    throw Diagnostic(DiagnosticLevel::ERROR, "Declaration can't have more than one sign specifier", location);
                sign_token = token;
            } else if (specifier->typedefName()) {
                if (identifier.has_value())
                    throw Diagnostic(DiagnosticLevel::ERROR, "Non-primitive types can't have more than one identifier", location);
                identifier = TypeIdentifier {.category = TypeCategory::TYPEDEF, .name = token};
            }
            else if (specifier->Complex())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Complex types are unsupported", location);
            else if (specifier->KW__m128() || specifier->KW__m128d() || specifier->KW__m128i())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "__m128 types are not implemented", location);
            else if (specifier->atomicTypeSpecifier())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Atomic type specifiers are unsupported", location);
            else if (specifier->structOrUnionSpecifier()) {
                if (identifier.has_value())
                    throw Diagnostic(DiagnosticLevel::ERROR, "Non-primitive types can't have more than one identifier", location);
                identifier = decode_struct_or_union_specifier(specifier->structOrUnionSpecifier());
            }
            else if (specifier->enumSpecifier())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Enums are not implemented", location);
            else if (specifier->typeofSpecifier())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Typeof specifiers are not implemented", location);
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown type specifier `{}`", specifier->getText()), location);
        }

        if (!identifier.has_value() && !sign_token.has_value() && primitive_type_tokens.empty())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "No valid type specifier found", base_location);

        if (identifier.has_value()) {
            if (sign_token.has_value() || !primitive_type_tokens.empty())
                throw Diagnostic(DiagnosticLevel::ERROR, "Non-primitive type name can't have primitive type specifiers", base_location);
            return identifier.value();
        }

        // ---- Now resolve primitive types. From now on `typedef_token` is empty

        // Bool is always alone
        if (primitive_type_tokens.contains("_Bool") || primitive_type_tokens.contains("bool")) {
            if (primitive_type_tokens.size() > 1 || sign_token.has_value())
                throw Diagnostic(DiagnosticLevel::ERROR, "Primitive type bool can't have other type specifiers", base_location);
            return {TypeCategory::PRIMITIVE, "bool"};
        }

        // Floating-point types
        if (primitive_type_tokens.contains("float") || primitive_type_tokens.contains("double")) {
            if (sign_token.has_value())
                throw Diagnostic(DiagnosticLevel::ERROR, "Floating-point type can't have a sign specifier", base_location);

            if (primitive_type_tokens.contains("float")) {
                if (primitive_type_tokens.size() > 1)
                    throw Diagnostic(DiagnosticLevel::ERROR, "Primitive type `float` can't have any other type specifiers", base_location);
                return {TypeCategory::PRIMITIVE, "float"};
            }

            // From now on, only double remains
            if (primitive_type_tokens.contains("long") && primitive_type_tokens.size() == 2)
                return {TypeCategory::PRIMITIVE, "long double"};
            else if (primitive_type_tokens.size() == 1)
                return {TypeCategory::PRIMITIVE, "double"};
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
            return {TypeCategory::PRIMITIVE, std::format("{} char", sign_token.value())};
        }

        // From now on, only short, int and long remain
        if (primitive_type_tokens.contains("short")) {
            if (primitive_type_tokens.contains("long"))
                throw Diagnostic(DiagnosticLevel::ERROR, "Primitive type `short` can't also be `long`", base_location);
            return {TypeCategory::PRIMITIVE, std::format("{} short int", sign_token.value())};
        }

        // From now on, only long and int remain
        const size_t nof_longs = primitive_type_tokens.count("long");
        switch (nof_longs) {
            case 0:  return {TypeCategory::PRIMITIVE, std::format("{} int", sign_token.value())};
            case 1:  return {TypeCategory::PRIMITIVE, std::format("{} long int", sign_token.value())};
            case 2:  return {TypeCategory::PRIMITIVE, std::format("{} long long int", sign_token.value())};
            default: throw Diagnostic(DiagnosticLevel::ERROR, "Primitive type specifier `long` can't be given more than twice", base_location);
        }
    }

    // Decode a struct or union specifier, push struct/enum/union definitions to the current scope and return the type identifier
    // Don't check whether named struct / unions exists
    TypeIdentifier Generator::decode_struct_or_union_specifier(CParser::StructOrUnionSpecifierContext* context) {
        const CodeLocation location = locate(context);

        if (context->attributeSpecifierSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Attribute specifiers are not supported", location);
        if (context->gnuAttributes())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GNU attributes are not supported", location);

        TypeIdentifier identifier;

        if (context->Identifier()) identifier.name = context->Identifier()->getText();
        else                       identifier.name = anonymous_identifier();

        if      (context->structOrUnion()->Struct())  identifier.category = TypeCategory::STRUCT;
        else if (context->structOrUnion()->Union())   identifier.category = TypeCategory::UNION;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown struct or union keyword `{}`", context->structOrUnion()->getText()), location);

        if (context->memberDeclarationList()) {
            std::shared_ptr<CompoundType> definition = std::make_shared<CompoundType>(identifier, location);
            current_scope()->add_type(definition);  // Push the incomplete type to the current scope to allow struct members to use it

            for (CParser::MemberDeclarationContext* declaration : context->memberDeclarationList()->memberDeclaration())
                definition->members.append_range(decode_member_declaration(declaration));

            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Complete structure declarations are not implemented", location);
            definition->is_complete = true;
        }

        return identifier;
    }


    std::vector<StructMember> Generator::decode_member_declaration(CParser::MemberDeclarationContext* context) {
        const CodeLocation location = locate(context);

        if (context->attributeSpecifierSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Attribute specifiers are not implemented", location);
        if (context->staticAssertDeclaration())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Static assertions are not supported", location);
        if (context->KW__extension__())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GNU extensions are not supported", location);

        TypeSpecification base_spec = decode_specifier_qualifier_list(context->specifierQualifierList());
        if (!context->memberDeclaratorList()) {
            StructMember member = {.name = anonymous_identifier(), .location = location, .spec = base_spec};
            return {member};
        } else {
            return decode_member_declarator_list(context->memberDeclaratorList(), base_spec);
        }
    }

    std::vector<StructMember> Generator::decode_member_declarator_list(CParser::MemberDeclaratorListContext* context, TypeSpecification base_spec) {
        if (!context->gnuAttributes().empty())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GNU attributes are not supported", locate(context));

        std::vector<StructMember> members;
        for (CParser::StructDeclaratorContext* declarator : context->structDeclarator())
            members.push_back(decode_struct_declarator(declarator, base_spec));

        return members;
    }

    StructMember Generator::decode_struct_declarator(CParser::StructDeclaratorContext* context, TypeSpecification base_spec) {
        const CodeLocation location = locate(context);
        if (context->gnuAttributes())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "GNU attributes are not implemented", location);
        if (context->constantExpression())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Bitfields are not implemented", location);
        else if (!context->declarator())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Anonymous bitfields are not implemented", location);

        TypeSpecification member_spec = base_spec;
        std::optional<std::string> name = decode_declarator(member_spec, context->declarator());
        std::string member_name = name.value_or(anonymous_identifier());
        return {.name = member_name, .location = location, .spec = member_spec};
    }

    std::optional<std::string> Generator::decode_declarator(TypeSpecification& spec, CParser::DeclaratorContext* context) {
        if (context->gnuAttribute())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GNU attributes are not implemented", locate(context));
        if (!context->gccDeclaratorExtension().empty())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GCC declarator extensions are not supported", locate(context));
        if (context->declarationSpecifiers())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Declarator-level specifiers are not implemented", locate(context));

        if (context->pointer())
            spec.pointer_spec.insert_range(spec.pointer_spec.cbegin(), decode_pointer_spec(context->pointer()));

        if (context->declarator())
            return decode_declarator(spec, context->declarator());
        else if (context->directDeclarator())
            return decode_direct_declarator(spec, context->directDeclarator());
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown declarator type `{}`", context->getText()));
    }

    std::optional<std::string> Generator::decode_direct_declarator(TypeSpecification& spec, CParser::DirectDeclaratorContext* context) {
        const CodeLocation location = locate(context);

        if (context->attributeSpecifierSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Attribute specifiers are not implemented", location);

        if (context->Identifier() && !context->DigitSequence())  // Identifier alternative
            return context->Identifier()->getText();
        else if (context->declarator() && context->LeftParen() && context->RightParen())  // Parenthesized alternative
            return decode_declarator(spec, context->declarator());
        else if (context->directDeclarator() && context->LeftBracket() && context->RightBracket())  // Array alternatives
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Array declarators are not implemented", location);
        else if (context->directDeclarator() && context->LeftParen() && context->RightParen())  // Function alternative
            return decode_function_direct_declarator(spec, context);
        else if (context->Identifier() && context->DigitSequence())  // Bitfield alternative
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Bitfields are not implemented", location);
        else if (context->vcSpecificModifer())  // VC-specific alternatives
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "VC declarator extensions are not supported", location);
        else if (context->gnuAttribute())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GNU attributes are not implemented", location);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown declarator type `{}`", context->getText()), location);
    }

    std::optional<std::string> Generator::decode_function_direct_declarator(TypeSpecification& spec, CParser::DirectDeclaratorContext* context) {
        if (context->attributeSpecifierSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Attribute specifiers are not implemented", locate(context));

        spec.is_function_type = true;
        std::optional<std::string> name = decode_direct_declarator(spec, context->directDeclarator());

        if (context->parameterTypeList())
            spec.parameters.append_range(decode_parameter_type_list(context->parameterTypeList()));
        // Otherwise, no parameters

        return name;
    }

    std::vector<Declaration> Generator::decode_parameter_type_list(CParser::ParameterTypeListContext* context) {
        if (context->Ellipsis())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Variadic functions are not implemented", locate(context));

        return decode_parameter_list(context->parameterList());
    }

    std::vector<Declaration> Generator::decode_parameter_list(CParser::ParameterListContext* context) {
        std::vector<Declaration> parameters;
        for (CParser::ParameterDeclarationContext* parameter : context->parameterDeclaration()) {
            Declaration declaration = decode_parameter_declaration(parameter);
            if (declaration.spec.is_void()) {
                if (!declaration.name.empty() || context->parameterDeclaration().size() > 1)
                    throw Diagnostic(DiagnosticLevel::ERROR, "Invalid void parameter declaration", locate(parameter));
            } else {
                parameters.push_back(declaration);  // Only add non-void parameters
            }
        }

        return parameters;
    }

    Declaration Generator::decode_parameter_declaration(CParser::ParameterDeclarationContext* context) {
        Declaration parameter;

        decode_declaration_specifiers(parameter, context->declarationSpecifiers());

        std::optional<std::string> name;
        if (context->declarator())
            name = decode_declarator(parameter.spec, context->declarator());
        else if (context->abstractDeclarator())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Abstract parameter declarators are not implemented", locate(context));
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "No declarator found in parameter declaration", locate(context));

        if (name.has_value())
            parameter.name = name.value();

        parameter.storage |= StorageClass::PARAMETER;
        return parameter;
    }

    std::vector<Flags<TypeQualifier>> Generator::decode_pointer_spec(CParser::PointerContext* context) {
        std::vector<Flags<TypeQualifier>> pointer_spec;
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
}
