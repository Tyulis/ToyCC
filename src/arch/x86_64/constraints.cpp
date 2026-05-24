#include "arch/x86_64/constraints.hpp"
#include "gen/execmodel/x86_64/location.h"

namespace toycc::arch::x86_64 {
    // Compute how many uses of the current value of `input_operand` remain after this match (excluding the current group match)
    static ssize_t get_remaining_liveness(const flow::DependencyGraph& graph, const ir::Operand& input_operand, const GroupMatch& group_match) {
        const std::unordered_set<std::shared_ptr<flow::DependencyNode>> group_statements(group_match.statements.begin(), group_match.statements.end());
        std::shared_ptr<ir::Declaration> input_variable = input_operand.declaration();
        std::shared_ptr<flow::DependencyNode> input_node = nullptr;
        for (std::shared_ptr<flow::DependencyNode> statement : group_statements) {
            for (std::shared_ptr<flow::DependencyNode> node : graph.previous_nodes(statement)) {
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
        for (const flow::DependencyGraph::Edge& edge : graph.out_edges(input_node))
            if (!group_statements.contains(edge.exit) || (edge.attr.type & flow::DependencyType::LIVE_ON_EXIT))
                remaining_liveness += 1;
        return remaining_liveness;
    }

    OperandMatch check_overwrite(const StackFrame& frame, const flow::DependencyGraph& graph, const ir::Operand& input_operand, const ir::Operand& output_operand, const GroupMatch& group_match) {
        using toycc::execmodel::x86_64::UNIQUE_LOCATIONS;

        if (input_operand.tag() == ir::Operand::CONSTANT)
            return {OperandMatch::REQUIRES_TRANSFER, UNIQUE_LOCATIONS};  // Can't overwrite a constant
        if (output_operand.tag() == ir::Operand::CONSTANT)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "A constant can't be an output", input_operand.location);

        if (input_operand.tag() == ir::Operand::DEREFERENCE && output_operand.tag() == ir::Operand::DEREFERENCE)
            return (input_operand == output_operand) ? OperandMatch {OperandMatch::OK, UNIQUE_LOCATIONS} : OperandMatch {OperandMatch::REQUIRES_TRANSFER, UNIQUE_LOCATIONS};

        if (input_operand.tag() == ir::Operand::DEREFERENCE || output_operand.tag() == ir::Operand::DEREFERENCE)
            return {OperandMatch::REQUIRES_TRANSFER, UNIQUE_LOCATIONS};

        ssize_t remaining_liveness = get_remaining_liveness(graph, input_operand, group_match);

        std::unordered_set<Location> current_locations = frame.locate(input_operand);
        remaining_liveness -= (current_locations.size() - 1);

        // Still live / only on the stack so can't overwrite
        if (remaining_liveness > 0 || current_locations.empty())
            return {OperandMatch::REQUIRES_TRANSFER, unordered_set_difference(UNIQUE_LOCATIONS, current_locations)};
        else
            return {OperandMatch::OK, UNIQUE_LOCATIONS};
    }

    OperandMatch check_implicit_overwrite(const StackFrame& frame, const flow::DependencyGraph& graph, const ir::Operand& input_operand, const GroupMatch& group_match, Location overwritten_location) {
        using toycc::execmodel::x86_64::UNIQUE_LOCATIONS;

        switch (input_operand.tag()) {
            case ir::Operand::CONSTANT:     return {OperandMatch::REQUIRES_TRANSFER, UNIQUE_LOCATIONS};  // Can't overwrite a constant
            case ir::Operand::DEREFERENCE:  return {OperandMatch::REQUIRES_TRANSFER, UNIQUE_LOCATIONS};
            case ir::Operand::VARIABLE: {
                ssize_t remaining_liveness = get_remaining_liveness(graph, input_operand, group_match);

                std::unordered_set<Location> current_locations = frame.locate(input_operand);

                // When the operand is not yet in its implicitly required location, it will be copied there while keeping its original locations,
                // so the overwritten_location is safe to overwrite
                if (!current_locations.contains(overwritten_location))
                    return {OperandMatch::OK, UNIQUE_LOCATIONS};

                remaining_liveness -= current_locations.size();

                // Still live / only on the stack so can't overwrite
                if (remaining_liveness > 0 || current_locations.empty())
                    return {OperandMatch::REQUIRES_TRANSFER, unordered_set_difference(UNIQUE_LOCATIONS, {overwritten_location})};
                else
                    return {OperandMatch::OK, UNIQUE_LOCATIONS};
            }
        }
        __builtin_unreachable();
    }
}
