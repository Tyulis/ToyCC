#include "diagnostic.h"
#include "ir/declaration.h"
#include "semantic/analyzer.h"
#include "util/strings.h"

namespace toycc::semantic {
    RValue SemanticAnalyzer::decode_constant(CParser::ConstantContext* context) {
        if (context->CharacterConstant())
            return decode_character_constant(context->CharacterConstant());
        else if (context->FloatingConstant())
            return decode_floating_constant(context->FloatingConstant());
        else if (context->IntegerConstant())
            return decode_integer_constant(context->IntegerConstant());
        else if (context->predefinedConstant())
            return decode_predefined_constant(context->predefinedConstant());
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown constant type `{}`", context->getText()), locate(context));
    }

    RValue SemanticAnalyzer::decode_character_constant(antlr4::tree::TerminalNode* terminal) {
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
        return Constant {IntegerConstant(text[0]), location, literal_character_type};
    }

    RValue SemanticAnalyzer::decode_floating_constant(antlr4::tree::TerminalNode* terminal) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Floating constants are not implemented", locate(terminal));
    }

    RValue SemanticAnalyzer::decode_integer_constant(antlr4::tree::TerminalNode* terminal) {
        const CodeLocation location = locate(terminal);
        std::string text = terminal->getText();

        const size_t suffix_position = text.find_first_of("uUlL");
        std::string suffix;
        if (suffix_position != std::string::npos) {
            suffix = text.substr(suffix_position);
            text = text.substr(0, suffix_position);
        }

        size_t value;
        if (text.at(0) == '0') {
            if (text.length() == 1)
                value = 0;
            else if (text.at(1) == 'x' || text.at(1) == 'X')
                value = std::stoull(text.substr(2), nullptr, 16);
            else if (text.at(1) == 'b' || text.at(1) == 'B')
                value = std::stoull(text.substr(2), nullptr, 2);
            else
                value = std::stoull(text.substr(1), nullptr, 8);
        } else {
            value = std::stoull(text, nullptr, 10);
        }

        to_lower_inplace(suffix);
        TypeIdentifier type_identifier = {.tag = TypeTag::DIRECT, .name = {}};

        if      (suffix.empty())                     { type_identifier.name = "signed int";             }
        else if (suffix == "u")                      { type_identifier.name = "unsigned int";           }
        else if (suffix == "l")                      { type_identifier.name = "signed long int";        }
        else if (suffix == "ll")                     { type_identifier.name = "signed long long int";   }
        else if (suffix == "ul" || suffix == "lu")   { type_identifier.name = "unsigned long int";      }
        else if (suffix == "ull" || suffix == "llu") { type_identifier.name = "unsigned long long int"; }
        else throw Diagnostic(DiagnosticLevel::ERROR, std::format("Unknown integer literal suffix `{}`", suffix), location);

        std::shared_ptr<Type> type = resolve_type(type_identifier, location);
        return Constant {IntegerConstant(value), location, type};
    }

    RValue SemanticAnalyzer::decode_predefined_constant(CParser::PredefinedConstantContext* context) {
        if (context->True())
            return Constant {.value = IntegerConstant(1), .location = locate(context), .type = boolean_type};
        else if (context->False())
            return Constant {.value = IntegerConstant(0), .location = locate(context), .type = boolean_type};
        else if (context->Nullptr())
            return Constant {.value = IntegerConstant(0), .location = locate(context), .type = void_pointer_type};
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Unknown predefined constant `{}`", context->getText()), locate(context));
    }

    RValue SemanticAnalyzer::decode_string_literal(std::vector<antlr4::tree::TerminalNode*> terminals) {
        std::string value;
        for (antlr4::tree::TerminalNode* terminal : terminals)
            value += decode_string_part(terminal);

        CodeLocation location = locate(terminals[0]);
        Constant array_length = Constant {IntegerConstant(value.size() + 1), location, literal_integer_type};

        // String literals are technically char* (not const) even though writing to them is undefined behaviour
        std::shared_ptr<Type> literal_string_type = ArrayType::make(anonymous_type(), location, character_type, array_length);
        return Constant {value, locate(terminals[0]), literal_string_type};
    }

    std::string SemanticAnalyzer::decode_string_part(antlr4::tree::TerminalNode* terminal) {
        std::string code = terminal->getText();

        std::string quote = "";
        std::string content = "";

        enum {QUOTE, STRING, ESCAPE} state = QUOTE;
        for (char character : code) {
            switch (state) {
                case QUOTE: {
                    if (character == '"')
                        state = STRING;
                    else if (!WHITESPACE.contains(character))
                        quote += character;
                    break;
                }

                case STRING: {
                    if (character == '\\') {
                        content += character;
                        state = ESCAPE;
                    } else if (character == '"') {
                        state = QUOTE;
                    } else {
                        content += character;
                    }
                    break;
                }

                case ESCAPE: {
                    content += character;
                    state = QUOTE;
                }
            }
        }

        if (!quote.empty())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Wide character strings are not implemented", locate(terminal));

        return unescape(content);
    }
}
