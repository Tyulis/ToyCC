#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/random_access_index.hpp>

#include "ir/type.h"
#include "ir/label.h"
#include "ir/declaration.h"

namespace toycc::ir {
    using namespace boost::multi_index;

    enum class ScopeType {
        GLOBAL, BLOCK, FUNCTION, LOOP, CONDITIONAL, SWITCH,
    };

    struct Statement;
    struct Scope {
        public:
            ScopeType type;
            std::shared_ptr<Declaration> function;
            std::string break_label;
            std::string continue_label;
            std::unordered_map<std::string, Label> labels;
            std::vector<Statement> statements;

            Scope(ScopeType type, std::shared_ptr<Declaration> function, std::string break_label = {}, std::string continue_label = {});

            std::string ir_code() const;

            std::shared_ptr<Type>        find_type(TypeIdentifier identifier);
            std::shared_ptr<Declaration> find_typedef(std::string name);
            std::shared_ptr<Declaration> find_local(std::string name);
            std::optional<Label>         find_label(std::string name);
            std::optional<Label>         find_label(const Statement& marker);

            std::shared_ptr<Type>        add_type(std::shared_ptr<Type> type);
            std::shared_ptr<Declaration> add_typedef(std::shared_ptr<Declaration> declaration);
            std::shared_ptr<Declaration> add_local(std::shared_ptr<Declaration> declaration);
            Statement&                   add_statement(const Statement& statement);
            Label&                       add_label(LabelType type, std::string name, CodeLocation location);
            Label&                       add_label(LabelType type, std::string name, const Statement& marker, CodeLocation location);
            Label&                       add_label(const Label& label);

            std::shared_ptr<Declaration> pop_local(std::string name);

            void clear_types();


            struct name_index_tag {};
            struct insertion_index_tag {};
            struct extract_declaration_name {
                using result_type = std::string;
                std::string operator() (std::shared_ptr<Declaration> decl) const {
                    return decl->name;
                }
            };

            using ordered_declaration_map = multi_index_container<std::shared_ptr<Declaration>,
                    indexed_by<random_access<tag<insertion_index_tag>>, hashed_unique<tag<name_index_tag>, extract_declaration_name>>>;
            using insertion_index = ordered_declaration_map::index<insertion_index_tag>::type;
            using name_index = ordered_declaration_map::index<name_index_tag>::type;

            insertion_index& locals_list();

        private:
            std::unordered_map<ir::TypeIdentifier, std::shared_ptr<Type>> types;
            ordered_declaration_map typedefs;
            ordered_declaration_map locals;
    };
}

#include "ir/statement.h"
