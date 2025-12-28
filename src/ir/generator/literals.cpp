#include "diagnostic.h"
#include "ir/generator.h"
#include "util/strings.h"

namespace toycc::ir {
    std::shared_ptr<Declaration> Generator::decode_constant(antlr4::tree::TerminalNode* terminal) {
        const std::string text = terminal->getText();
        if (text.starts_with("'") || text.starts_with("L'") || text.starts_with("u'") || text.starts_with("U'"))
            return decode_character_constant(terminal);
        else if (text.contains("."))
            return decode_floating_constant(terminal);
        else
            return decode_integer_constant(terminal);
    }

    std::shared_ptr<Declaration> Generator::decode_character_constant(antlr4::tree::TerminalNode* terminal) {
        const CodeLocation location = locate(terminal);
        std::string text = terminal->getText();

        // Remove the quote
        switch (text[0]) {
            case 'u':
            case 'U':
            case 'L':
                text.erase(0, 2);
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Wide character literals are not implemented", location);
            case '\'':
                text.erase(0, 1);
                break;
            default:
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown character constant prefix `{}`", text[0]), location);
        }
        text.erase(text.length() - 1, 1);

        try {
            unescape_inplace(text);
        } catch (const std::runtime_error& exc) {
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Invalid character constant : {}", exc.what()), location);
        }

        if (text.length() > 1)
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Character literal must resolve to a single character"), location);

        // Declare the character constant
        TypeIdentifier type_identifier = {.category = TypeCategory::PRIMITIVE, .name = "signed char"};
        stmt::LoadConst::Constant value = static_cast<ssize_t>(text[0]);

        TypeSpecification spec = resolve_type(type_identifier, location);
        std::shared_ptr<Declaration> declaration = declare_temporary(spec, location);

        current_scope()->add_statement(std::make_shared<stmt::LoadConst> (location, declaration, value));
        return declaration;
    }

    std::shared_ptr<Declaration> Generator::decode_floating_constant(antlr4::tree::TerminalNode* terminal) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Floating constants are not implemented", locate(terminal));
    }

    std::shared_ptr<Declaration> Generator::decode_integer_constant(antlr4::tree::TerminalNode* terminal) {
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

    std::shared_ptr<Declaration> Generator::decode_decimal_constant(antlr4::tree::TerminalNode* terminal) {
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

    std::shared_ptr<Declaration> Generator::decode_hexadecimal_constant(antlr4::tree::TerminalNode* terminal) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Hexadecimal constants are not implemented", locate(terminal));
    }

    std::shared_ptr<Declaration> Generator::decode_binary_constant(antlr4::tree::TerminalNode* terminal) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Binary constants are not implemented", locate(terminal));
    }

    std::shared_ptr<Declaration>Generator::decode_octal_constant(antlr4::tree::TerminalNode* terminal) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Octal constants are not implemented", locate(terminal));
    }


    std::shared_ptr<Declaration> Generator::decode_string_literal(std::vector<antlr4::tree::TerminalNode*> terminals) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "String literals are not implemented", locate(terminals[0]));
    }


    std::shared_ptr<Declaration> Generator::declare_integer_constant(size_t base_value, std::string suffix, CodeLocation location) {
        to_lower_inplace(suffix);
        TypeIdentifier type_identifier = {.category = TypeCategory::PRIMITIVE, .name = ""};
        stmt::LoadConst::Constant value;

        if      (suffix.empty())                     { value = static_cast<ssize_t> (base_value); type_identifier.name = "signed int";             }
        else if (suffix == "u")                      { value = static_cast<size_t>  (base_value); type_identifier.name = "unsigned int";           }
        else if (suffix == "l")                      { value = static_cast<ssize_t> (base_value); type_identifier.name = "signed long int";        }
        else if (suffix == "ll")                     { value = static_cast<ssize_t> (base_value); type_identifier.name = "signed long long int";   }
        else if (suffix == "ul" || suffix == "lu")   { value = static_cast<size_t>  (base_value); type_identifier.name = "unsigned long int";      }
        else if (suffix == "ull" || suffix == "llu") { value = static_cast<size_t>  (base_value); type_identifier.name = "unsigned long long int"; }
        else throw Diagnostic(DiagnosticLevel::ERROR, std::format("Unknown integer literal suffix `{}`", suffix), location);

        TypeSpecification spec = resolve_type(type_identifier, location);
        std::shared_ptr<Declaration> declaration = declare_temporary(spec, location);

        current_scope()->add_statement(std::make_shared<stmt::LoadConst> (location, declaration, value));
        return declaration;
    }
}
