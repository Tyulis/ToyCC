#include <list>
#include <sstream>
#include <algorithm>

#include "diagnostic.h"
#include "ir/scope.h"
#include "util/strings.h"

namespace toycc::ir {
    Scope::Scope(ScopeType type, std::shared_ptr<Declaration> function, std::string entry_label, std::string exit_label)
        : type(type), function(function), entry_label(entry_label), exit_label(exit_label) {}

    std::string Scope::ir_code() const {
        std::vector<std::list<std::string>> label_positions(statements.size() + 1);
        for (std::pair<std::string, Label> label : labels) {
            auto it = std::ranges::find(statements, label.second.target);
            if (it == statements.end())
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Marker for label `{}` not found in the current scope", label.second.name), label.second.location);
            label_positions[it - statements.begin()].push_back(label.first);
        }

        std::stringstream code;
        for (std::pair<TypeIdentifier, std::shared_ptr<Type>> item : types)
            code << "#type " << item.second->ir_code() << " " << item.second->name << ";\n";
        for (std::shared_ptr<Declaration> item : typedefs)
            code << item->ir_code() << ";\n";
        for (std::shared_ptr<Declaration> item : locals)
            code << item->ir_code() << ";\n";

        for (size_t statement_index = 0; statement_index < statements.size(); statement_index++) {
            for (std::string label : label_positions[statement_index])
                code << label << ":\n";
            std::string statement_code = statements[statement_index]->ir_code();
            if (!statement_code.empty())
                code << statement_code << ";\n";
        }
        for (std::string label : label_positions[statements.size()])
            code << label << ":\n";
        return rtrim(code.str());
    }

    std::shared_ptr<Type> Scope::find_type(TypeIdentifier identifier) {
        auto element = types.find(identifier);
        if (element == types.end())  return nullptr;
        else                         return element->second;
    }

    std::shared_ptr<Declaration> Scope::find_typedef(std::string name) {
        auto& index = typedefs.get<name_index_tag>();
        auto element = index.find(name);
        if (element == index.end())  return nullptr;
        else                         return *element;
    }

    std::shared_ptr<Declaration> Scope::find_local(std::string name) {
        auto& index = locals.get<name_index_tag>();
        auto element = index.find(name);
        if (element == index.end())  return nullptr;
        else                         return *element;
    }


    std::shared_ptr<Type> Scope::add_type(std::shared_ptr<Type> type) {
        auto existing_type = types.find(type->identifier());
        if (existing_type != types.end())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Type `{}` is already defined in this scope", type->identifier().name), type->location)
                  .add_note(DiagnosticLevel::NOTE, "Already defined here", existing_type->second->location);

        types[type->identifier()] = type;
        return type;
    }

    std::shared_ptr<Declaration> Scope::add_typedef(std::shared_ptr<Declaration> declaration) {
        std::shared_ptr<Declaration> existing_typedef = find_typedef(declaration->name);
        if (existing_typedef.get() != nullptr)
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Typedef `{}` is already defined in this scope", declaration->name), declaration->location)
                  .add_note(DiagnosticLevel::NOTE, "Already defined here", existing_typedef->location);
        typedefs.push_back(declaration);
        return declaration;
    }

    std::shared_ptr<Declaration> Scope::add_local(std::shared_ptr<Declaration> declaration) {
        std::shared_ptr<Declaration> existing_decl = find_local(declaration->name);
        if (existing_decl.get() != nullptr)
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Name `{}` is already defined in this scope", declaration->name), declaration->location)
                  .add_note(DiagnosticLevel::NOTE, "Already defined here", existing_decl->location);
        locals.push_back(declaration);
        return declaration;
    }

    std::shared_ptr<Statement> Scope::add_statement(std::shared_ptr<Statement> statement) {
        statements.push_back(statement);
        return statement;
    }

    size_t Scope::add_label(LabelType type, std::string name, std::string source_name, CodeLocation location) {
        auto existing_label = labels.find(name);
        if (existing_label != labels.end())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Label `{}` already exists in this scope", name));

        std::shared_ptr<Statement> marker = add_statement(std::make_shared<stmt::Marker>(location));
        labels[name] = Label {.type = type, .name = name, .source_name = source_name, .target = marker, .location = location};
        return statements.size();
    }

    std::shared_ptr<Declaration> Scope::pop_local(std::string name) {
        auto& index = locals.get<name_index_tag>();
        auto element = index.find(name);
        if (element == index.end()) {
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Attempted to pop local name `{}`, not found in this scope", name));
        } else {
            std::shared_ptr<Declaration> declaration = *element;
            index.erase(element);
            return declaration;
        }

    }

    void Scope::clear_types() {
        types.clear();
        typedefs.clear();
    }

    Scope::insertion_index& Scope::locals_list() {
        return locals.get<insertion_index_tag>();
    }
}
