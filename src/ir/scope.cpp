#include <list>
#include <sstream>

#include "diagnostic.h"
#include "ir/scope.h"
#include "util/strings.h"

namespace toycc::ir {
    Scope::Scope(ScopeType type, std::shared_ptr<Declaration> function) : type(type), function(function) {}

    std::string Scope::ir_code() const {
        std::vector<std::list<std::string>> label_positions(statements.size() + 1);
        for (std::pair<std::string, size_t> label : labels)
            label_positions[label.second].push_back(label.first);

        std::stringstream code;
        for (std::pair<TypeIdentifier, std::shared_ptr<Type>> item : types)
            code << item.second->ir_code() << ";\n";
        for (std::shared_ptr<Declaration> item : typedefs)
            code << item->ir_code() << ";\n";
        for (std::shared_ptr<Declaration> item : locals)
            code << item->ir_code() << ";\n";

        for (size_t statement_index = 0; statement_index < statements.size(); statement_index++) {
            for (std::string label : label_positions[statement_index])
                code << label << ":\n";
            code << statements[statement_index]->ir_code() << ";\n";
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
        auto& index = typedefs.get<name_index>();
        auto element = index.find(name);
        if (element == index.end())  return nullptr;
        else                         return *element;
    }

    std::shared_ptr<Declaration> Scope::find_local(std::string name) {
        auto& index = locals.get<name_index>();
        auto element = index.find(name);
        if (element == index.end())  return nullptr;
        else                         return *element;
    }


    std::shared_ptr<Type> Scope::add_type(std::shared_ptr<Type> type) {
        auto existing_type = types.find(type->identifier);
        if (existing_type != types.end())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Type `{}` is already defined in this scope", type->identifier.name), type->location)
                  .add_note(DiagnosticLevel::NOTE, "Already defined here", existing_type->second->location);

        types[type->identifier] = type;
        return type;
    }

    std::shared_ptr<Declaration> Scope::add_typedef(std::shared_ptr<Declaration> declaration) {
        auto& index = typedefs.get<name_index>();
        auto existing_typedef = index.find(declaration->name);
        if (existing_typedef != index.end())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Typedef `{}` is already defined in this scope", declaration->name), declaration->location)
                  .add_note(DiagnosticLevel::NOTE, "Already defined here", (*existing_typedef)->location);
        typedefs.push_back(declaration);
        return declaration;
    }

    std::shared_ptr<Declaration> Scope::add_local(std::shared_ptr<Declaration> declaration) {
        auto& index = locals.get<name_index>();
        auto existing_decl = index.find(declaration->name);
        if (existing_decl != index.end())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Name `{}` is already defined in this scope", declaration->name), declaration->location)
            .add_note(DiagnosticLevel::NOTE, "Already defined here", (*existing_decl)->location);
        locals.push_back(declaration);
        return declaration;
    }

    std::shared_ptr<Statement> Scope::add_statement(std::shared_ptr<Statement> statement) {
        statements.push_back(statement);
        return statement;
    }

    size_t Scope::add_label(std::string label) {
        auto existing_label = labels.find(label);
        if (existing_label != labels.end())
            throw Diagnostic(DiagnosticLevel::ERROR, std::format("Label `{}` already exists in this scope", label));
        labels[label] = statements.size();
        return statements.size();
    }
}
