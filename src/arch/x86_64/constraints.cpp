#include "arch/x86_64/constraints.hpp"
#include "gen/execmodel/x86_64/location.h"

namespace toycc::arch::x86_64 {
    static ssize_t get_remaining_liveness(const ir::DependencyGraph& graph, const ir::Operand& input_operand, const GroupMatch& group_match) {
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
        return remaining_liveness;
    }

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

        ssize_t remaining_liveness = get_remaining_liveness(graph, input_operand, group_match);

        std::unordered_set<Location> current_locations = frame.locate(input_operand);
        remaining_liveness -= current_locations.size();

        // Still live / only on the stack so can't overwrite
        if (remaining_liveness > 0 || current_locations.empty())
            return {OperandMatch::REQUIRES_TRANSFER, unordered_set_difference(UNIQUE_LOCATIONS, current_locations)};
        else
            return {OperandMatch::OK, UNIQUE_LOCATIONS};
    }

    OperandMatch check_implicit_overwrite(const StackFrame& frame, const ir::DependencyGraph& graph, const ir::Operand& input_operand, const GroupMatch& group_match, Location overwritten_location) {
        using toycc::execmodel::x86_64::UNIQUE_LOCATIONS;

        if (input_operand.is_constant() || input_operand.is_label())
            return {OperandMatch::REQUIRES_TRANSFER, UNIQUE_LOCATIONS};  // Can't overwrite a constant
        if (input_operand.is_dereference())
            return OperandMatch {OperandMatch::REQUIRES_TRANSFER, UNIQUE_LOCATIONS};

        ssize_t remaining_liveness = get_remaining_liveness(graph, input_operand, group_match);

        std::unordered_set<Location> current_locations = frame.locate(input_operand);
        remaining_liveness -= current_locations.size();

        // Still live / only on the stack so can't overwrite
        if (remaining_liveness > 0 || current_locations.empty())
            return {OperandMatch::REQUIRES_TRANSFER, unordered_set_difference(UNIQUE_LOCATIONS, {overwritten_location})};
        else
            return {OperandMatch::OK, UNIQUE_LOCATIONS};
    }
}
