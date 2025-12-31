#include <sstream>

#include "diagnostic.h"
#include "ir/scope.h"
#include "util/strings.h"

namespace toycc::ir {
    Scope::Scope(ScopeType type, std::shared_ptr<Declaration> function, std::string entry_label, std::string exit_label)
        : type(type), function(function), entry_label(entry_label), exit_label(exit_label) {}

    std::string Scope::ir_code() const {
        std::stringstream code;
        for (std::pair<TypeIdentifier, std::shared_ptr<Type>> item : types)
            code << "#type " << item.second->ir_code() << " " << item.second->name << ";\n";
        for (std::shared_ptr<Declaration> item : typedefs)
            code << item->ir_code() << ";\n";
        for (std::shared_ptr<Declaration> item : locals)
            code << item->ir_code() << ";\n";

        for (std::shared_ptr<Statement> statement : statements) {
            const auto [begin, end] = markers.equal_range(statement);
            for (auto label_it = begin; label_it != end; label_it++)
                code << label_it->second->name << ": ";
            std::string statement_code = statement->ir_code();
            if (!statement_code.empty())
                code << statement_code << ";\n";
        }
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

        auto existing_label = labels.find(declaration->name);
        if (existing_label != labels.end())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Name `{}` is already used for a label", declaration->name))
                  .add_note(DiagnosticLevel::NOTE, "Already defined here", existing_label->second->location);

        locals.push_back(declaration);
        return declaration;
    }

    std::shared_ptr<Statement> Scope::add_statement(std::shared_ptr<Statement> statement) {
        statements.push_back(statement);
        return statement;
    }

    std::shared_ptr<Label> Scope::add_label(std::shared_ptr<Label> label) {
        auto existing_label = labels.find(label->name);
        if (existing_label != labels.end())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Label `{}` already exists in this scope", label->name), label->location)
                  .add_note(DiagnosticLevel::NOTE, "Already defined here", existing_label->second->location);

        std::shared_ptr<Declaration> existing_decl = find_local(label->name);
        if (existing_decl.get() != nullptr)
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Label name `{}` is already used for a declaration in this scope", label->name), label->location)
                  .add_note(DiagnosticLevel::NOTE, "Already defined here", existing_decl->location);

        labels.insert({label->name, label});
        markers.insert({label->marker, label});
        return label;
    }

    std::shared_ptr<Label> Scope::add_label(LabelType type, std::string name, std::shared_ptr<Statement> marker, CodeLocation location) {
        std::shared_ptr<Label> label = std::make_shared<Label>(type, name, marker, location);
        return add_label(label);
    }

    std::shared_ptr<Label> Scope::add_label(LabelType type, std::string name, CodeLocation location) {
        std::shared_ptr<Statement> marker = add_statement(Statement::make_marker(location));
        return add_label(type, name, marker, location);
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
