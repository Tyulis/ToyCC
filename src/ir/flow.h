#pragma once

#include <memory>
#include <vector>

#include "ir/label.h"
#include "ir/statement.h"
#include "ir/declaration.h"

#include "util/graph.hpp"

namespace toycc::ir {
    using DependencyGraph = Graph<Statement, std::unordered_set<std::shared_ptr<Declaration>>>;

    enum class LocalBlockType {
        ENTRY, INNER, EXIT,
    };

    struct LocalBlock {
        public:
            LocalBlockType type;
            std::shared_ptr<Label> label;
            DependencyGraph statements;
            std::unordered_set<std::shared_ptr<Declaration>> input_variables;
            std::unordered_set<std::shared_ptr<Declaration>> output_variables;

            LocalBlock(LocalBlockType type, std::shared_ptr<Label> label = nullptr);

            std::string ir_code() const;
            void add_statement(std::shared_ptr<Statement> statement, std::unordered_set<std::shared_ptr<Declaration>> available_decls);

        private:
            std::unordered_map<std::shared_ptr<Declaration>, std::shared_ptr<Statement>> last_modification;
    };

    enum class FlowType {
        FALLTHROUGH, JUMP,
    };

    using FlowGraph = Graph<LocalBlock, FlowType>;

    struct Procedure {
        public:
            std::shared_ptr<Declaration> declaration;
            CodeLocation location;
            FlowGraph blocks;
            std::shared_ptr<LocalBlock> entry_block;
            std::shared_ptr<LocalBlock> exit_block;
            std::vector<std::shared_ptr<Declaration>> parameters;
            std::unordered_set<std::shared_ptr<Declaration>> locals;
            std::unordered_set<std::shared_ptr<Declaration>> globals;

            Procedure() = default;
            Procedure(std::shared_ptr<Statement> function);

            std::string ir_code() const;

        private:
            void find_globals(std::shared_ptr<Scope> scope);
            void find_globals(std::shared_ptr<Statement> statement);
            void build_flow_graph(std::shared_ptr<Scope> scope);
    };

    struct TranslationUnit {
        std::unordered_map<std::string, std::shared_ptr<Declaration>> globals;
        std::unordered_map<std::string, Procedure> procedures;

        std::string ir_code() const;
    };
}
