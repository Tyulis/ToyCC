#include "diagnostic.h"
#include "ir_generator.h"
#include "ir/type.h"
#include "arch/x86_64.h"

namespace toycc {
    static inline std::shared_ptr<ir::PrimitiveType> make_primitive_type(std::string name, bool is_signed, ir::PrimitiveSemantic semantic, size_t size, size_t alignment) {
        return std::make_shared<ir::PrimitiveType> (name, is_signed, semantic, size, alignment);
    }

    IRGenerator::IRGenerator(const SourceMap& source_map) : source_map(source_map), global_scope(std::make_shared<ir::Scope>()) {
        // Initialize the global scope
        using namespace toycc::arch;
        global_scope->types["void"]                   = make_primitive_type("void",                   false, ir::PrimitiveSemantic::VOID,    0,                0);
        global_scope->types["_Bool"]                  = make_primitive_type("_Bool",                  false, ir::PrimitiveSemantic::BOOL,    BOOL_SIZE,        BOOL_ALIGNMENT);
        global_scope->types["signed char"]            = make_primitive_type("signed char",            true,  ir::PrimitiveSemantic::INTEGER, CHAR_SIZE,        CHAR_ALIGNMENT);
        global_scope->types["unsigned char"]          = make_primitive_type("unsigned char",          false, ir::PrimitiveSemantic::INTEGER, CHAR_SIZE,        CHAR_ALIGNMENT);
        global_scope->types["signed short int"]       = make_primitive_type("signed short int",       true,  ir::PrimitiveSemantic::INTEGER, SHORT_SIZE,       SHORT_ALIGNMENT);
        global_scope->types["unsigned short int"]     = make_primitive_type("unsigned short int",     false, ir::PrimitiveSemantic::INTEGER, SHORT_SIZE,       SHORT_ALIGNMENT);
        global_scope->types["signed int"]             = make_primitive_type("signed int",             true,  ir::PrimitiveSemantic::INTEGER, INT_SIZE,         INT_ALIGNMENT);
        global_scope->types["unsigned int"]           = make_primitive_type("unsigned int",           false, ir::PrimitiveSemantic::INTEGER, INT_SIZE,         INT_ALIGNMENT);
        global_scope->types["signed long int"]        = make_primitive_type("signed long int",        true,  ir::PrimitiveSemantic::INTEGER, LONG_SIZE,        LONG_ALIGNMENT);
        global_scope->types["unsigned long int"]      = make_primitive_type("unsigned long int",      false, ir::PrimitiveSemantic::INTEGER, LONG_SIZE,        LONG_ALIGNMENT);
        global_scope->types["signed long long int"]   = make_primitive_type("signed long long int",   false, ir::PrimitiveSemantic::INTEGER, LONG_LONG_SIZE,   LONG_LONG_ALIGNMENT);
        global_scope->types["unsigned long long int"] = make_primitive_type("unsigned long long int", false, ir::PrimitiveSemantic::INTEGER, LONG_LONG_SIZE,   LONG_LONG_ALIGNMENT);
        global_scope->types["float"]                  = make_primitive_type("float",                  true,  ir::PrimitiveSemantic::FLOAT,   FLOAT_SIZE,       FLOAT_ALIGNMENT);
        global_scope->types["double"]                 = make_primitive_type("double",                 true,  ir::PrimitiveSemantic::FLOAT,   DOUBLE_SIZE,      DOUBLE_ALIGNMENT);
        global_scope->types["long double"]            = make_primitive_type("long double",            true,  ir::PrimitiveSemantic::FLOAT,   LONG_DOUBLE_SIZE, LONG_DOUBLE_ALIGNMENT);

        // Aliases to the primitive types
        global_scope->types["char"] = global_scope->types["signed char"];
        global_scope->types["short"] = global_scope->types["signed short int"];
        global_scope->types["short int"] = global_scope->types["signed short int"];
        global_scope->types["signed short"] = global_scope->types["signed short int"];
        global_scope->types["unsigned short"] = global_scope->types["unsigned short int"];
        global_scope->types["int"] = global_scope->types["signed int"];
        global_scope->types["signed"] = global_scope->types["signed int"];
        global_scope->types["unsigned"] = global_scope->types["unsigned int"];
        global_scope->types["long"] = global_scope->types["signed long int"];
        global_scope->types["long int"] = global_scope->types["signed long int"];
        global_scope->types["signed long"] = global_scope->types["signed long int"];
        global_scope->types["unsigned long"] = global_scope->types["unsigned long int"];
        global_scope->types["long long"] = global_scope->types["signed long long int"];
        global_scope->types["long long int"] = global_scope->types["signed long long int"];
        global_scope->types["signed long long"] = global_scope->types["signed long long int"];
        global_scope->types["unsigned long long"] = global_scope->types["unsigned long long int"];

        scope_stack.push_back(global_scope);
    }

    // ------------ Listener

    void IRGenerator::exitDeclarationDeclaration(CParser::DeclarationDeclarationContext* context) {
        std::vector<CParser::DeclarationSpecifierContext*> specifiers = context->declarationSpecifiers()->declarationSpecifier();
        if (specifiers[0]->storageClassSpecifier() && specifiers[0]->storageClassSpecifier()->Typedef())
            return register_typedef(context);

        throw Diagnostic(Diagnostic::Level::NOT_IMPLEMENTED, std::format("Unsupported declaration"), get_location(context));
    }


    // ------------ IR generation functions
    void IRGenerator::register_typedef(CParser::DeclarationDeclarationContext* context) {
        CodeLocation location = get_location(context);
        std::vector<CParser::DeclarationSpecifierContext*> specifiers = context->declarationSpecifiers()->declarationSpecifier();

        if (!specifiers.front()->storageClassSpecifier() || !specifiers.front()->storageClassSpecifier()->Typedef())
            throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, "Typedef declaration doesn't start with the `typedef` specifier", location);
        specifiers.erase(specifiers.begin());

        if (!specifiers.back()->typeSpecifier() || !specifiers.back()->typeSpecifier()->typedefName())
            throw Diagnostic(Diagnostic::Level::ERROR, "The last specifier in a typedef declaration must be an identifier", location);

        std::string name = specifiers.back()->typeSpecifier()->typedefName()->Identifier()->getText();
        specifiers.pop_back();

        std::stringstream target;
        for (CParser::DeclarationSpecifierContext* specifier : specifiers) {
            if (!specifier->typeSpecifier())
                throw Diagnostic(Diagnostic::Level::ERROR, std::format("Specifier `{}` is forbidden in a typedef declaration", specifier->getText()), location);

            target << specifier->getText();  // FIXME
        }

        scope_stack.back()->typedefs[name] = std::make_shared<ir::Typedef>(name, location, target.str());
    }

    // ------------ Internals
    CodeLocation IRGenerator::get_location(antlr4::ParserRuleContext* context) const {
        antlr4::Token* start_token = context->getStart();
        LinePosition line = source_map.at(start_token->getLine());
        return {.filename = line.filename, .line = line.line, .character = start_token->getCharPositionInLine()};
    }

    std::string IRGenerator::anonymous_identifier() {
        return std::format("<anonymous_{}>", unique_id++);
    }

    std::shared_ptr<ir::Type> IRGenerator::resolve_type(std::string name) noexcept {
        for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); it++) {
            std::shared_ptr<ir::Scope> scope = *it;

            while (scope->typedefs.contains(name))
                name = scope->typedefs.at(name)->target;

            if (scope->types.contains(name))
                return scope->types.at(name);
        }

        return nullptr;
    }

    std::shared_ptr<ir::Type> IRGenerator::get_type(std::string name, CodeLocation location) {
        std::deque<std::shared_ptr<ir::Typedef>> typedef_stack;

        for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); it++) {
            std::shared_ptr<ir::Scope> scope = *it;

            while (scope->typedefs.contains(name)) {
                std::shared_ptr<ir::Typedef> definition = scope->typedefs.at(name);
                typedef_stack.push_back(definition);
                name = definition->target;
            }

            if (scope->types.contains(name))
                return scope->types.at(name);
        }

        Diagnostic diagnostic(Diagnostic::Level::ERROR, std::format("Type with name {} is undefined", name), location);
        for (std::shared_ptr<ir::Typedef> definition : typedef_stack)
            diagnostic.add_note(Diagnostic::Level::NOTE, std::format("Typedef redirects {} to {}", definition->name, definition->target), definition->location);
        throw diagnostic;
    }
}
