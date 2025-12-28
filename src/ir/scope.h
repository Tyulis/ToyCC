#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/random_access_index.hpp>

#include "ir/type.h"
#include "ir/statement.h"
#include "ir/declaration.h"

namespace toycc::ir {
    using namespace boost::multi_index;

    enum class ScopeType {
        GLOBAL, BLOCK, FUNCTION, LOOP, CONDITIONAL, SWITCH,
    };

    struct Scope {
        public:
            ScopeType type;
            std::shared_ptr<Declaration> function;

            Scope(ScopeType type, std::shared_ptr<Declaration> function);

            std::string ir_code() const;

            std::shared_ptr<Type>        find_type(TypeIdentifier identifier);
            std::shared_ptr<Declaration> find_typedef(std::string name);
            std::shared_ptr<Declaration> find_local(std::string name);

            std::shared_ptr<Type>        add_type(std::shared_ptr<Type> type);
            std::shared_ptr<Declaration> add_typedef(std::shared_ptr<Declaration> declaration);
            std::shared_ptr<Declaration> add_local(std::shared_ptr<Declaration> declaration);
            std::shared_ptr<Statement>   add_statement(std::shared_ptr<Statement> statement);
            size_t add_label(std::string label);

        private:
            struct insertion_index {};
            struct name_index {};

            struct extract_declaration_name {
                using result_type = std::string;
                std::string operator() (std::shared_ptr<Declaration> decl) const {
                    return decl->name;
                }
            };

            using ordered_declaration_map = multi_index_container<std::shared_ptr<Declaration>,
                    indexed_by<random_access<tag<insertion_index>>, hashed_unique<tag<name_index>, extract_declaration_name>>>;

            std::unordered_map<ir::TypeIdentifier, std::shared_ptr<Type>> types;
            ordered_declaration_map typedefs;
            ordered_declaration_map locals;
            std::vector<std::shared_ptr<Statement>> statements;
            std::unordered_map<std::string, size_t> labels;  // Label -> index in `statements`
    };
}
