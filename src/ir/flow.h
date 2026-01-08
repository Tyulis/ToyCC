#pragma once

#include <memory>
#include <vector>
#include <variant>

#include "ir/label.h"
#include "ir/statement.h"
#include "ir/declaration.h"

#include "util/graph.hpp"

namespace toycc::ir {
    struct DependencyNode {
        std::variant<Statement, std::shared_ptr<Declaration>> node;

        bool is_statement() const;
        bool is_value() const;
        CodeLocation location() const;

        Statement& statement();
        const Statement& statement() const;
        std::shared_ptr<Declaration> declaration() const;

        bool operator== (const DependencyNode& rhs) const;
        bool operator== (std::shared_ptr<Declaration> rhs) const;
    };

    using DependencyGraph = Graph<DependencyNode>;

    enum class LocalBlockType {
        ENTRY, INNER, EXIT,
    };

    struct LocalBlock {
        public:
            LocalBlockType type;
            std::optional<Label> label;
            DependencyGraph dependencies;

            LocalBlock(LocalBlockType type, std::optional<Label> label = {});

            std::string dot_subgraph(std::stringstream& dot, std::string cluster_name) const;
            void add_statement(const Statement& statement, std::unordered_set<std::shared_ptr<Declaration>> available_decls);

        private:
            std::unordered_map<std::shared_ptr<Declaration>, std::shared_ptr<DependencyNode>> last_modification;
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
            Procedure(const Statement& function);

            std::string dot_subgraph(std::stringstream& dot) const;

        private:
            void find_globals(std::shared_ptr<Scope> scope);
            void find_globals(const Statement& statement);
            void build_flow_graph(std::shared_ptr<Scope> scope);
    };

    struct TranslationUnit {
        std::unordered_map<std::string, std::shared_ptr<Declaration>> globals;
        std::unordered_map<std::string, Procedure> procedures;

        std::string dot_graph() const;
    };
}
