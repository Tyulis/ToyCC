#pragma once

#include <memory>
#include <vector>
#include <variant>
#include <armadillo>

#include "ir/label.h"
#include "ir/statement.h"
#include "ir/declaration.h"

#include "util/flags.hpp"
#include "util/graph.hpp"

namespace toycc::ir {
    enum class DependencyType {
        READ         = 0x01,
        WRITE        = 0x02,
        CALL         = 0x04,
        DEREFERENCE  = 0x08,
        LIVE_ON_EXIT = 0x10,
    };

    enum class OperandGroup {
        INDIRECT, INPUT, OUTPUT
    };

    struct Dependency {
        Flags<DependencyType> type;
        OperandGroup operand_group = OperandGroup::INDIRECT;  // Which kind of operand requires this dependency (INPUT or OUTPUT)
        size_t operand_index = std::numeric_limits<size_t>::max();
    };

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

    using DependencyGraph = Graph<DependencyNode, Dependency>;

    struct DependencyMatrix {
        std::vector<std::shared_ptr<DependencyNode>> statements;  // Rows
        std::vector<std::shared_ptr<DependencyNode>> values;      // Columns
        arma::imat matrix;  // Matrix with n for statement.inputs[n-1], -n for statement.outputs[n-1], 0 when unlinked
    };
    DependencyMatrix to_dependency_matrix(const DependencyGraph& graph);

    enum class BasicBlockType {
        ENTRY, INNER, EXIT,
    };

    struct BasicBlock {
        public:
            BasicBlockType type;
            std::optional<Label> label;
            DependencyGraph dependencies;

            BasicBlock(BasicBlockType type, std::shared_ptr<size_t> unique_id, std::optional<Label> label = {});

            std::string dot_subgraph(std::stringstream& dot, std::string cluster_name) const;

            void add_statement(const Statement& statement, std::unordered_set<std::shared_ptr<Declaration>> defined_decls);
            void finish();
            void not_live_on_exit(const std::unordered_set<std::shared_ptr<Declaration>>& intermediate);
            void split_intermediate_values();

            std::unordered_set<std::shared_ptr<Declaration>> locals() const;
            std::unordered_set<std::shared_ptr<Declaration>> live_on_entry() const;
            std::unordered_set<std::shared_ptr<Declaration>> live_on_exit() const;

        private:
            std::shared_ptr<size_t> unique_id;
            std::shared_ptr<DependencyNode> exit_statement = nullptr;
            std::unordered_map<std::shared_ptr<Declaration>, std::shared_ptr<DependencyNode>> last_modification;

            std::shared_ptr<Declaration> declare_intermediate(std::shared_ptr<Type> type, CodeLocation location);
    };

    enum class FlowType {
        FALLTHROUGH, JUMP,
    };

    using FlowGraph = Graph<BasicBlock, FlowType>;

    struct Procedure {
        public:
            std::shared_ptr<Declaration> declaration;
            CodeLocation location;
            FlowGraph blocks;
            std::shared_ptr<BasicBlock> entry_block;
            std::shared_ptr<BasicBlock> exit_block;
            std::vector<std::shared_ptr<Declaration>> parameters;

            Procedure() = default;
            Procedure(const Statement& function, const std::unordered_set<std::shared_ptr<Declaration>>& globals, std::shared_ptr<size_t> unique_id);

            std::string dot_subgraph(std::stringstream& dot) const;

            std::unordered_set<std::shared_ptr<Declaration>> locals() const;

        private:
            std::shared_ptr<size_t> unique_id;

            void build_flow_graph(std::shared_ptr<Scope> scope, const std::unordered_set<std::shared_ptr<Declaration>>& globals);
            void resolve_intermediates();
    };

    struct TranslationUnit {
        std::unordered_set<std::shared_ptr<Declaration>> globals;
        std::unordered_map<std::string, Procedure> procedures;
        std::shared_ptr<size_t> unique_id = 0;

        TranslationUnit() = default;
        TranslationUnit(std::shared_ptr<Scope> global_scope);

        std::string dot_graph() const;
    };
}
