#pragma once

#include <limits>
#include <memory>
#include <variant>
#include <armadillo>
#include <unordered_map>

#include "code_location.h"
#include "ir/declaration.h"
#include "ir/label.h"
#include "ir/statement.h"
#include "util/flags.hpp"
#include "util/graph.hpp"


namespace toycc::flow {
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
        std::variant<ir::Statement, std::shared_ptr<ir::Declaration>> node;

        bool is_statement() const;
        bool is_value() const;
        CodeLocation location() const;

        ir::Statement& statement();
        const ir::Statement& statement() const;
        std::shared_ptr<ir::Declaration> declaration() const;

        bool operator== (const DependencyNode& rhs) const;
        bool operator== (std::shared_ptr<ir::Declaration> rhs) const;
    };

    using DependencyGraph = Graph<DependencyNode, Dependency>;
    std::string dot_graph(const DependencyGraph& graph, std::string cluster_name);

    struct DependencyMatrix {
        std::vector<std::shared_ptr<DependencyNode>> statements;  // Rows
        std::vector<std::shared_ptr<DependencyNode>> values;      // Columns
        arma::imat matrix;  // Matrix with n for statement.inputs[n-1], -n for statement.outputs[n-1], 0 when unlinked
    };
    DependencyMatrix to_dependency_matrix(const DependencyGraph& graph);
    std::ostream& operator<< (std::ostream& stream, const DependencyMatrix& graph);

    enum class BasicBlockType {
        ENTRY, INNER, EXIT,
    };

    struct BasicBlock {
        public:
            BasicBlockType type;
            std::optional<ir::Label> label;
            DependencyGraph dependencies;
            std::shared_ptr<DependencyNode> exit_statement = nullptr;

            BasicBlock(BasicBlockType type, std::shared_ptr<size_t> unique_id, std::optional<ir::Label> label = {});

            std::string dot_subgraph(std::stringstream& dot, std::string cluster_name) const;

            void add_statement(const ir::Statement& statement, std::unordered_set<std::shared_ptr<ir::Declaration>> defined_decls);
            void finish();
            void not_live_on_exit(const std::unordered_set<std::shared_ptr<ir::Declaration>>& intermediate);
            void split_intermediate_values();

            std::unordered_set<std::shared_ptr<ir::Declaration>> locals() const;
            std::unordered_set<std::shared_ptr<ir::Declaration>> live_on_entry() const;
            std::unordered_set<std::shared_ptr<ir::Declaration>> live_on_exit() const;

            bool has_calls() const;

        private:
            std::shared_ptr<size_t> unique_id;
            std::unordered_map<std::shared_ptr<ir::Declaration>, std::shared_ptr<DependencyNode>> last_modification;

            std::shared_ptr<ir::Declaration> declare_intermediate(std::shared_ptr<ir::Type> type, CodeLocation location);
    };

}
