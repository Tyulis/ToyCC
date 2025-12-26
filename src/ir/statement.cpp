#include <format>
#include <sstream>

#include "diagnostic.h"
#include "ir/statement.h"
#include "util/strings.h"

namespace toycc::ir {
    Statement::Statement(stmt::Tag tag, CodeLocation location) : tag(tag), location(location) {}

    std::string Statement::tag_repr() const {
        switch (tag) {
            case stmt::Tag::NOP:         return "NOP";
            case stmt::Tag::BLOCK:       return "BLOCK";
            case stmt::Tag::FUNCTION:    return "FUNCTION";
            case stmt::Tag::LOAD_CONST:  return "LOAD_CONST";
            case stmt::Tag::RETURN:      return "RETURN";
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid statement tag");
    }
}

namespace toycc::ir::stmt {
    Block::Block(CodeLocation location, std::shared_ptr<Scope> scope) : Statement(Tag::BLOCK, location), scope(scope) {}
    Block::Block(Tag tag, CodeLocation location, std::shared_ptr<Scope> scope) : Statement(tag, location), scope(scope) {}
    std::string Block::ir_code() const {
        return std::format("{} {{\n{}\n}}", tag_repr(), indent(scope->ir_code(), true, "    "));
    }

    Function::Function(CodeLocation location, std::shared_ptr<Scope> scope, std::shared_ptr<Declaration> declaration) : Block(Tag::FUNCTION, location, scope), declaration(declaration) {}
    std::string Function::ir_code() const {
        return std::format("{} {} {{\n{}\n}}", tag_repr(), declaration->name, indent(scope->ir_code(), true, "    "));
    }

    Return::Return(CodeLocation location) : Statement(Tag::RETURN, location), declaration(nullptr) {}
    Return::Return(CodeLocation location, std::shared_ptr<Declaration> declaration) : Statement(Tag::RETURN, location), declaration(declaration) {}
    std::string Return::ir_code() const {
        return std::format("{} {}", tag_repr(), declaration->name);
    }

    LoadConst::LoadConst(CodeLocation location, std::shared_ptr<Declaration> destination, Constant value)
            : Statement(Tag::LOAD_CONST, location), destination(destination), value(value) {}
    std::string LoadConst::ir_code() const {
        std::stringstream code;
        code << tag_repr() << " " << destination->name << " ";
        std::visit([&](auto&& val) { code << val; }, value);
        return code.str();
    }
}
