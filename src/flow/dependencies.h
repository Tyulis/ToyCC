#pragma once

#include <limits>
#include <memory>
#include <variant>
#include <expected>
#include <optional>
#include <armadillo>

#include "ir/declaration.h"
#include "ir/statement.h"
#include "util/flags.hpp"
#include "util/graph.hpp"

namespace toycc::flow {
    // -------- Types to define dependency graphs

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

    enum class ValueStatus {
        UNKNOWN,        // The value of that variable couldn't be inferred at compile-time
        UNINITIALIZED,  // That variable wasn't initialized at all
    };

    using FoldedValue = std::expected<ir::Constant, ValueStatus>;

    struct Dependency {
        Flags<DependencyType> type;
        OperandGroup operand_group = OperandGroup::INDIRECT;  // Which kind of operand requires this dependency (INPUT or OUTPUT)
        size_t operand_index = std::numeric_limits<size_t>::max();
    };

    std::ostream& operator<< (std::ostream& stream, DependencyType value);
    std::ostream& operator<< (std::ostream& stream, OperandGroup value);
    std::ostream& operator<< (std::ostream& stream, const Dependency& dependency);

    struct ValueNode {
        std::shared_ptr<ir::Declaration> variable;
        FoldedValue value;
    };

    class DependencyNode {
        public:
            DependencyNode(const ir::Statement& statement);
            DependencyNode(std::shared_ptr<ir::Declaration> variable);
            DependencyNode(std::shared_ptr<ir::Declaration> variable, ir::Constant value);

            bool is_statement() const;
            bool is_value() const;
            CodeLocation location() const;

            ir::Statement& statement();
            const ir::Statement& statement() const;
            std::shared_ptr<ir::Declaration> declaration() const;
            const FoldedValue& value() const;
            FoldedValue& value();

            bool operator== (const DependencyNode& rhs) const;
            bool operator== (std::shared_ptr<ir::Declaration> rhs) const;

        private:
            std::variant<ir::Statement, ValueNode> node;
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

    using ConstantMap = std::unordered_map<std::shared_ptr<ir::Declaration>, FoldedValue>;
}
