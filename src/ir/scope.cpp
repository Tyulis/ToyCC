#include <sstream>

#include "diagnostic.h"
#include "ir/scope.h"
#include "util/strings.h"

namespace toycc::ir {
    Scope::Scope(ScopeType type, std::shared_ptr<Declaration> function, std::string break_label, std::string continue_label)
        : type(type), function(function), break_label(break_label), continue_label(continue_label) {}

    std::string Scope::ir_code() const {
        std::stringstream code;
        for (std::pair<TypeIdentifier, std::shared_ptr<Type>> item : types)
            code << "#type " << item.second->ir_code() << " " << item.second->repr() << ";\n";
        for (std::shared_ptr<Declaration> item : typedefs)
            code << item->ir_code() << ";\n";
        for (std::shared_ptr<Declaration> item : locals)
            code << item->ir_code() << ";\n";

        for (const Statement& statement : statements) {
            if (statement.tag == StatementTag::MARKER)
                code << labels.at(statement.output->label()).name << ":\n";
            else
                code << statement.ir_code() << ";\n";
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

    std::optional<Label> Scope::find_label(std::string name) {
        auto element = labels.find(name);
        if (element == labels.end()) return {};
        else                         return element->second;
    }

    std::optional<Label> Scope::find_label(const Statement& marker) {
        if (marker.tag != StatementTag::MARKER || !marker.output.has_value() || !marker.output->is_label())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid marker", marker.location);

        const std::string& name = marker.output.value().label();
        return find_label(name);
    }

    std::shared_ptr<Type> Scope::add_type(std::shared_ptr<Type> type) {
        auto existing_type = types.find(type->identifier());
        if (existing_type != types.end() && existing_type->second->complete())  // Incomplete type declarations may be overridden by the complete declaration
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
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Name `{}` is already used for a label", declaration->name), declaration->location)
                  .add_note(DiagnosticLevel::NOTE, "Already defined here", existing_label->second.location);

        locals.push_back(declaration);
        return declaration;
    }

    Statement& Scope::add_statement(const Statement& statement) {
        statements.push_back(statement);
        return statements.back();
    }

    Label& Scope::add_label(const Label& label) {
        auto existing_label = labels.find(label.name);
        if (existing_label != labels.end())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Label `{}` already exists in this scope", label.name), label.location)
                  .add_note(DiagnosticLevel::NOTE, "Already defined here", existing_label->second.location);

        std::shared_ptr<Declaration> existing_decl = find_local(label.name);
        if (existing_decl.get() != nullptr)
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Label name `{}` is already used for a declaration in this scope", label.name), label.location)
                  .add_note(DiagnosticLevel::NOTE, "Already defined here", existing_decl->location);

        return labels[label.name] = label;
    }

    Label& Scope::add_label(LabelType type, std::string name, CodeLocation location) {
        add_statement(Statement::make_marker(location, name));
        return add_label(Label {type, name, location});
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
