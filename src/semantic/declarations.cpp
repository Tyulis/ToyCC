#include "code_location.h"
#include "diagnostic.h"
#include "ir/type_expressions.h"
#include "semantic/analyzer.h"

namespace toycc::semantic {
    // Decode a declaration and push it to the current scope
    void SemanticAnalyzer::decode_declaration(CParser::DeclarationContext* context) {
        if (context->staticAssertDeclaration())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Static assertions are not implemented", locate(context));
        if (context->attributeDeclaration())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Attribute declarations are not implemented", locate(context));

        decode_declaration(context->declarationSpecifiers(), context->initDeclaratorList());
    }

    void SemanticAnalyzer::decode_declaration(CParser::DeclarationSpecifiersContext* specifiers, CParser::InitDeclaratorListContext* init_declarators) {
        Declaration base_declaration;
        base_declaration.location = locate(specifiers);
        decode_declaration_specifiers(base_declaration, specifiers);

        // First, only process the declarations
        std::vector<Declaration> declarations;
        if (init_declarators) {
            for (CParser::InitDeclaratorContext* declarator : init_declarators->initDeclarator()) {
                Declaration declaration = base_declaration;
                decode_declarator(declaration, declarator->declarator());
                declarations.push_back(declaration);
            }
        } else {
            declarations.push_back(base_declaration);
        }

        std::vector<std::shared_ptr<Declaration>> declared_variables;
        for (const Declaration& declaration : declarations) {
            declaration.check();
            declared_variables.push_back(declare(declaration));
        }

        // Then the initializations
        if (init_declarators) {
            for (unsigned decl_index = 0; decl_index < declarations.size(); decl_index++) {
                std::shared_ptr<Declaration> declaration = declared_variables[decl_index];
                CParser::InitDeclaratorContext* declarator = init_declarators->initDeclarator()[decl_index];
                if (declarator->initializer()) {
                    if (declaration->storage & StorageClass::TYPEDEF)
                        throw Diagnostic(DiagnosticLevel::ERROR, "Initializers are not allowed in typedef declarations", locate(declarator->initializer()));

                    const CodeLocation initializer_location = locate(declarator->initializer());
                    std::shared_ptr<ExpressionResult> initializer = decode_initializer(declarator->initializer());
                    emit_copy(declaration, initializer->operand(), initializer_location, true);
                }
            }
        }
    }

    void SemanticAnalyzer::decode_for_declaration(CParser::ForDeclarationContext* context) {
        decode_declaration(context->declarationSpecifiers(), context->initDeclaratorList());
    }

    void SemanticAnalyzer::decode_declaration_specifiers(Declaration& declaration, CParser::DeclarationSpecifiersContext* specifiers) {
        std::vector<CParser::TypeSpecifierContext*> type_specifiers;
        Flags<TypeQualifier> qualifiers;
        std::optional<size_t> custom_alignment_bits;

        for (CParser::DeclarationSpecifierContext* specifier : specifiers->declarationSpecifier()) {
            if (specifier->storageClassSpecifier())
                declaration.storage |= decode_storage_class(specifier->storageClassSpecifier());
            else if (specifier->typeSpecifier())
                type_specifiers.push_back(specifier->typeSpecifier());
            else if (specifier->typeQualifier())
                qualifiers |= decode_type_qualifier(specifier->typeQualifier());
            else if (specifier->functionSpecifier())
                declaration.function_spec |= decode_function_specifier(specifier->functionSpecifier());
            else if (specifier->alignmentSpecifier())
                custom_alignment_bits = resolve_alignment_specifier(specifier->alignmentSpecifier());
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown declaration specifier `{}`", specifier->getText()), locate(specifier));
        }

        if (!declaration.storage)  // No specific keyword used -> auto storage duration
            declaration.storage = StorageClass::AUTO;

        const bool is_typedef = declaration.storage & StorageClass::TYPEDEF;
        declaration.type = resolve_type_specifiers(type_specifiers, is_typedef);
        if (qualifiers)
            declaration.type = QualifiedType::make(anonymous_type(), declaration.location, declaration.type, qualifiers);
        if (custom_alignment_bits.has_value())
            declaration.type = AlignedType::make(anonymous_type(), declaration.location, declaration.type, custom_alignment_bits.value());
    }

    std::shared_ptr<Type> SemanticAnalyzer::decode_specifier_qualifier_list(CParser::SpecifierQualifierListContext* context) {
        const CodeLocation location = locate(context);

        if (context->gnuAttributes())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GNU attributes are not supported", location);
        if (context->attributeSpecifierSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Attribute specifiers are not supported", location);

        std::vector<CParser::TypeSpecifierQualifierContext*> specifiers = context->typeSpecifierQualifier();
        std::vector<CParser::TypeSpecifierContext*> type_specifiers;
        Flags<TypeQualifier> qualifiers;
        std::optional<size_t> custom_alignment_bits;
        for (CParser::TypeSpecifierQualifierContext* specifier : specifiers) {
            if (specifier->typeSpecifier())
                type_specifiers.push_back(specifier->typeSpecifier());
            else if (specifier->typeQualifier())
                qualifiers |= decode_type_qualifier(specifier->typeQualifier());
            else if (specifier->alignmentSpecifier())
                custom_alignment_bits = resolve_alignment_specifier(specifier->alignmentSpecifier());
            else
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown specifier qualifier `{}`", specifier->getText()), locate(specifier));
        }

        std::shared_ptr<Type> type = resolve_type_specifiers(type_specifiers, false);
        if (qualifiers)
            type = QualifiedType::make(anonymous_type(), location, type, qualifiers);
        if (custom_alignment_bits.has_value())
            type = AlignedType::make(anonymous_type(), location, type, custom_alignment_bits.value());
        return type;
    }

    Flags<StorageClass> SemanticAnalyzer::decode_storage_class(CParser::StorageClassSpecifierContext* context) {
        if      (context->Typedef())      return StorageClass::TYPEDEF;
        else if (context->Extern())       return StorageClass::EXTERN;
        else if (context->Static())       return StorageClass::STATIC;
        else if (context->ThreadLocal())  return StorageClass::THREAD_LOCAL;
        else if (context->Auto())         return StorageClass::AUTO;
        else if (context->Register())     return StorageClass::REGISTER;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown storage class `{}`", context->getText()), locate(context));
    }

    Flags<TypeQualifier> SemanticAnalyzer::decode_type_qualifier_list(CParser::TypeQualifierListContext* context) {
        Flags<TypeQualifier> qualifiers;
        for (CParser::TypeQualifierContext* qualifier : context->typeQualifier())
            qualifiers |= decode_type_qualifier(qualifier);
        return qualifiers;
    }

    Flags<TypeQualifier> SemanticAnalyzer::decode_type_qualifier(CParser::TypeQualifierContext* context) {
        if      (context->Const())     return TypeQualifier::CONST;
        else if (context->Restrict())  return TypeQualifier::RESTRICT;
        else if (context->Volatile())  return TypeQualifier::VOLATILE;
        else if (context->Atomic())    return TypeQualifier::ATOMIC;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown type qualifier `{}`", context->getText()), locate(context));
    }

    Flags<FunctionSpecifier> SemanticAnalyzer::decode_function_specifier(CParser::FunctionSpecifierContext* context) {
        if      (context->Inline() || context->KW__inline__()) return FunctionSpecifier::INLINE;
        else if (context->Noreturn())                          return FunctionSpecifier::NORETURN;
        else if (context->KW__stdcall())                       return FunctionSpecifier::STDCALL;
        else if (context->gnuAttribute()) throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GNU attributes are not supported", locate(context));
        else if (context->KW__declspec()) throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Declspec specifiers are not supported", locate(context));
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown function specifier `{}`", context->getText()), locate(context));
    }

    size_t SemanticAnalyzer::resolve_alignment_specifier(CParser::AlignmentSpecifierContext* context) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Alignment specifiers are not implemented", locate(context));
    }

    std::shared_ptr<Type> SemanticAnalyzer::resolve_type_specifiers(std::vector<CParser::TypeSpecifierContext*> type_specifiers, bool is_typedef) {
        const CodeLocation location = locate(type_specifiers[0]);

        TypeIdentifier identifier = decode_type_specifiers(type_specifiers);
        std::shared_ptr<Type> type = resolve_type_without_error(identifier);

        // Declare incomplete types in typedefs
        if (type.get() == nullptr && is_typedef && (identifier.tag == TypeTag::STRUCT || identifier.tag == TypeTag::UNION || identifier.tag == TypeTag::ENUM)) {
            switch (identifier.tag) {
                case TypeTag::STRUCT:  type = current_scope()->add_type(StructType::make(identifier.name, location, false));                 break;
                case TypeTag::UNION:   type = current_scope()->add_type(UnionType::make (identifier.name, location, false));                 break;
                case TypeTag::ENUM:    type = current_scope()->add_type(EnumType::make  (identifier.name, location, enum_underlying_type));  break;
                default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid type tag in incomplete typedef push", location);
            }
        }

        if (type.get() == nullptr)
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Type `{}` was not declared", identifier.text()), location);

        return type;
    }

    // Decode a type specifier, push eventual anonymous struct/enum/union declarations to the current scope and return the type identifier
    // Don't check whether non-primitive named types exists
    TypeIdentifier SemanticAnalyzer::decode_type_specifiers(std::vector<CParser::TypeSpecifierContext*> type_specifiers) {
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
                identifier = TypeIdentifier {.tag = TypeTag::DIRECT, .name = token};
            } else if (specifier->Char() || specifier->Short() || specifier->Int() || specifier->Long() || specifier->Float() || specifier->Double() || specifier->Bool()) {
                primitive_type_tokens.insert(token);
            } else if (specifier->Signed() || specifier->Unsigned()) {
                if (sign_token.has_value())
                    throw Diagnostic(DiagnosticLevel::ERROR, "Declaration can't have more than one sign specifier", location);
                sign_token = token;
            } else if (specifier->typedefName()) {
                if (identifier.has_value())
                    throw Diagnostic(DiagnosticLevel::ERROR, "Non-primitive types can't have more than one identifier", location);
                identifier = TypeIdentifier {.tag = TypeTag::TYPEDEF, .name = token};
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
            return {.tag = TypeTag::DIRECT, .name = "bool"};
        }

        // Floating-point types
        if (primitive_type_tokens.contains("float") || primitive_type_tokens.contains("double")) {
            if (sign_token.has_value())
                throw Diagnostic(DiagnosticLevel::ERROR, "Floating-point type can't have a sign specifier", base_location);

            if (primitive_type_tokens.contains("float")) {
                if (primitive_type_tokens.size() > 1)
                    throw Diagnostic(DiagnosticLevel::ERROR, "Primitive type `float` can't have any other type specifiers", base_location);
                return {.tag = TypeTag::DIRECT, .name = "float"};
            }

            // From now on, only double remains
            if (primitive_type_tokens.contains("long") && primitive_type_tokens.size() == 2)
                return {.tag = TypeTag::DIRECT, .name = "long double"};
            else if (primitive_type_tokens.size() == 1)
                return {.tag = TypeTag::DIRECT, .name = "double"};
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
            return {.tag = TypeTag::DIRECT, .name = std::format("{} char", sign_token.value())};
        }

        // From now on, only short, int and long remain
        if (primitive_type_tokens.contains("short")) {
            if (primitive_type_tokens.contains("long"))
                throw Diagnostic(DiagnosticLevel::ERROR, "Primitive type `short` can't also be `long`", base_location);
            return {.tag = TypeTag::DIRECT, .name = std::format("{} short int", sign_token.value())};
        }

        // From now on, only long and int remain
        const size_t nof_longs = primitive_type_tokens.count("long");
        switch (nof_longs) {
            case 0:  return {.tag = TypeTag::DIRECT, .name = std::format("{} int", sign_token.value())};
            case 1:  return {.tag = TypeTag::DIRECT, .name = std::format("{} long int", sign_token.value())};
            case 2:  return {.tag = TypeTag::DIRECT, .name = std::format("{} long long int", sign_token.value())};
            default: throw Diagnostic(DiagnosticLevel::ERROR, "Primitive type specifier `long` can't be given more than twice", base_location);
        }
    }

    // Decode a struct or union specifier, push struct/enum/union definitions to the current scope and return the type identifier
    // Don't check whether named struct / unions exists
    TypeIdentifier SemanticAnalyzer::decode_struct_or_union_specifier(CParser::StructOrUnionSpecifierContext* context) {
        const CodeLocation location = locate(context);

        if (context->attributeSpecifierSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Attribute specifiers are not supported", location);
        if (context->gnuAttributes())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GNU attributes are not supported", location);

        TypeIdentifier identifier;

        if (context->Identifier()) identifier.name = context->Identifier()->getText();
        else                       identifier.name = anonymous_identifier();

        if      (context->structOrUnion()->Struct())  identifier.tag = TypeTag::STRUCT;
        else if (context->structOrUnion()->Union())   identifier.tag = TypeTag::UNION;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown struct or union keyword `{}`", context->structOrUnion()->getText()), location);

        if (context->memberDeclarationList()) {
            // Push the incomplete type to the current scope to allow struct members to use it
            std::shared_ptr<CompoundType> definition;
            switch (identifier.tag) {
                case TypeTag::STRUCT:  definition = StructType::make(identifier.name, location);  break;
                case TypeTag::UNION:   definition = UnionType::make (identifier.name, location);  break;
                default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid type category for a compound type", location);
            }
            current_scope()->add_type(definition);

            for (CParser::MemberDeclarationContext* declaration : context->memberDeclarationList()->memberDeclaration())
                definition->members.append_range(decode_member_declaration(declaration));

            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Complete structure declarations are not implemented", location);
            definition->is_complete = true;
        }

        return identifier;
    }


    std::vector<Member> SemanticAnalyzer::decode_member_declaration(CParser::MemberDeclarationContext* context) {
        const CodeLocation location = locate(context);

        if (context->attributeSpecifierSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Attribute specifiers are not implemented", location);
        if (context->staticAssertDeclaration())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Static assertions are not supported", location);
        if (context->KW__extension__())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GNU extensions are not supported", location);

        std::shared_ptr<Type> type = decode_specifier_qualifier_list(context->specifierQualifierList());
        if (!context->memberDeclaratorList())
            return {Member {anonymous_identifier(), type, location}};
        else
            return decode_member_declarator_list(context->memberDeclaratorList(), type);
    }

    std::vector<Member> SemanticAnalyzer::decode_member_declarator_list(CParser::MemberDeclaratorListContext* context, std::shared_ptr<Type> base_type) {
        if (!context->gnuAttributes().empty())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GNU attributes are not supported", locate(context));

        std::vector<Member> members;
        for (CParser::StructDeclaratorContext* declarator : context->structDeclarator())
            members.push_back(decode_struct_declarator(declarator, base_type));

        return members;
    }

    Member SemanticAnalyzer::decode_struct_declarator(CParser::StructDeclaratorContext* context, std::shared_ptr<Type> base_type) {
        const CodeLocation location = locate(context);
        if (context->gnuAttributes())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "GNU attributes are not implemented", location);
        if (context->constantExpression())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Bitfields are not implemented", location);
        else if (!context->declarator())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Anonymous bitfields are not implemented", location);

        Member member({}, base_type, location);
        decode_declarator(member, context->declarator());
        if (member.name.empty())
            member.name = anonymous_identifier();
        return member;
    }

    // Decode a member or variable declarator, updates its type with the qualifiers found in the declarator, and may update its name if one is provided
    void SemanticAnalyzer::decode_declarator(Member& member, CParser::DeclaratorContext* context) {
        if (context->gnuAttribute())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GNU attributes are not implemented", locate(context));
        if (!context->gccDeclaratorExtension().empty())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GCC declarator extensions are not supported", locate(context));
        if (context->declarationSpecifiers())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Declarator-level specifiers are not implemented", locate(context));

        if (context->pointer())
            member.type = decode_pointer_spec(context->pointer(), member.type);

        if (context->declarator())
            return decode_declarator(member, context->declarator());
        else if (context->directDeclarator())
            return decode_direct_declarator(member, context->directDeclarator());
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown declarator type `{}`", context->getText()));
    }

    void SemanticAnalyzer::decode_direct_declarator(Member& member, CParser::DirectDeclaratorContext* context) {
        const CodeLocation location = locate(context);

        if (context->attributeSpecifierSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Attribute specifiers are not implemented", location);

        if (context->Identifier() && !context->DigitSequence())  // Identifier alternative
            member.name = context->Identifier()->getText();
        else if (context->declarator() && context->LeftParen() && context->RightParen())  // Parenthesized alternative
            decode_declarator(member, context->declarator());
        else if (context->directDeclarator() && context->LeftBracket() && context->RightBracket())  // Array alternatives
            return decode_array_direct_declarator(member, context);
        else if (context->directDeclarator() && context->LeftParen() && context->RightParen())  // Function alternative
            return decode_function_direct_declarator(member, context);
        else if (context->Identifier() && context->DigitSequence())  // Bitfield alternative
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Bitfields are not implemented", location);
        else if (context->vcSpecificModifer())  // VC-specific alternatives
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "VC declarator extensions are not supported", location);
        else if (context->gnuAttribute())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "GNU attributes are not implemented", location);
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown declarator type `{}`", context->getText()), location);
    }

    void SemanticAnalyzer::decode_array_direct_declarator(Member& member, CParser::DirectDeclaratorContext* context) {
        if (context->Static())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Static variables as array length are not implemented", locate(context));
        if (context->Star())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Stars in array lengths are not implemented", locate(context));
        if (context->typeQualifierList())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Type qualifiers in array lengths are not implemented", locate(context));
        if (context->attributeSpecifierSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Attribute specifiers are not implemented", locate(context));

        decode_direct_declarator(member, context->directDeclarator());
        std::shared_ptr<ExpressionResult> length = decode_assignment_expression(context->assignmentExpression());
        member.type = ArrayType::make(anonymous_type(), locate(context), member.type, length->operand());
    }

    void SemanticAnalyzer::decode_function_direct_declarator(Member& member, CParser::DirectDeclaratorContext* context) {
        if (context->attributeSpecifierSequence())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Attribute specifiers are not implemented", locate(context));

        decode_direct_declarator(member, context->directDeclarator());
        std::shared_ptr<FunctionType> function_type = FunctionType::make(anonymous_type(), locate(context), member.type);
        if (context->parameterTypeList())
            function_type->parameters = decode_parameter_type_list(context->parameterTypeList());
        // Otherwise, no parameters

        member.type = function_type;
    }

    std::vector<Member> SemanticAnalyzer::decode_parameter_type_list(CParser::ParameterTypeListContext* context) {
        if (context->Ellipsis())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Variadic functions are not implemented", locate(context));

        return decode_parameter_list(context->parameterList());
    }

    std::vector<Member> SemanticAnalyzer::decode_parameter_list(CParser::ParameterListContext* context) {
        std::vector<Member> parameters;
        for (CParser::ParameterDeclarationContext* parameter : context->parameterDeclaration())
            parameters.push_back(decode_parameter_declaration(parameter));

        return parameters;
    }

    Member SemanticAnalyzer::decode_parameter_declaration(CParser::ParameterDeclarationContext* context) {
        Declaration parameter;
        decode_declaration_specifiers(parameter, context->declarationSpecifiers());

        // The syntax allows it, but the semantics don't
        if (parameter.storage.without(StorageClass::AUTO) || parameter.function_spec)
            throw Diagnostic(DiagnosticLevel::ERROR, "Function parameters can't have storage classes or function specifiers", locate(context));

        std::optional<std::string> name;
        if (context->declarator())
            decode_declarator(parameter, context->declarator());
        else if (context->abstractDeclarator())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Abstract parameter declarators are not implemented", locate(context));
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "No declarator found in parameter declaration", locate(context));

        if (name.has_value())
            parameter.name = name.value();

        return static_cast<Member>(parameter);
    }

    std::shared_ptr<Type> SemanticAnalyzer::decode_pointer_spec(CParser::PointerContext* context, std::shared_ptr<Type> type) {
        for (CParser::PointerLevelContext* level : context->pointerLevel()) {
            if (level->Caret())
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Carets in pointer specification are not supported", locate(level));

            type = PointerType::make(anonymous_type(), locate(level), type);
            if (level->typeQualifierList())
                type = QualifiedType::make(anonymous_type(), locate(level), type, decode_type_qualifier_list(level->typeQualifierList()));
        }

        return type;
    }
}
