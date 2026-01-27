#include "arch/x86_64/constraints.hpp"
#include "gen/execmodel/x86_64/location.h"

namespace toycc::arch::x86_64 {
    OperandMatch check_overwrite(const StackFrame& frame, const ir::DependencyGraph& graph, const ir::Operand& input_operand, const ir::Operand& output_operand, const GroupMatch& group_match) {
        using toycc::execmodel::x86_64::UNIQUE_LOCATIONS;

        if (input_operand.is_constant() || input_operand.is_label())
            return {OperandMatch::REQUIRES_TRANSFER, UNIQUE_LOCATIONS};  // Can't overwrite a constant
        if (output_operand.is_constant() || output_operand.is_label())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "A constant or label can't be an output", input_operand.location);

        if (input_operand.is_dereference() && output_operand.is_dereference())
            return (input_operand == output_operand) ? OperandMatch {OperandMatch::OK, UNIQUE_LOCATIONS} : OperandMatch {OperandMatch::REQUIRES_TRANSFER, UNIQUE_LOCATIONS};

        if (input_operand.is_dereference() || output_operand.is_dereference())
            return {OperandMatch::REQUIRES_TRANSFER, UNIQUE_LOCATIONS};

        const std::unordered_set<std::shared_ptr<ir::DependencyNode>> group_statements(group_match.statements.begin(), group_match.statements.end());
        std::shared_ptr<ir::Declaration> input_variable = input_operand.declaration();
        std::shared_ptr<ir::DependencyNode> input_node = nullptr;
        for (std::shared_ptr<ir::DependencyNode> statement : group_statements) {
            for (std::shared_ptr<ir::DependencyNode> node : graph.previous_nodes(statement)) {
                if (node->is_value() && node->declaration() == input_variable) {
                    input_node = node;
                    goto exit_find_input_node;
                }
            }
        }
        exit_find_input_node:;

        if (input_node.get() == nullptr)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "The input variable is not found in the dependency graph", input_operand.location);

        ssize_t remaining_liveness = 0;
        for (const ir::DependencyGraph::Edge& edge : graph.out_edges(input_node))
            if (!group_statements.contains(edge.exit) || (edge.attr.type & ir::DependencyType::LIVE_ON_EXIT))
                remaining_liveness += 1;

        std::unordered_set<Location> preferred_location;
        for (Location location : frame.locate(input_variable)) {
            remaining_liveness -= 1;
            if (UNIQUE_LOCATIONS.contains(location))
                preferred_location.insert(location);
        }

        // Still live / only on the stack so can't overwrite
        if (remaining_liveness > 0 || preferred_location.empty())
            return {OperandMatch::REQUIRES_TRANSFER, UNIQUE_LOCATIONS};
        else
            return {OperandMatch::OK, preferred_location};
    }
}
