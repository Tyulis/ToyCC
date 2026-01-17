#pragma once

#include "diagnostic.h"
#include "ir/flow.h"
#include "ir/type.h"
#include "ir/declaration.h"
#include "arch/x86_64/execmodel.h"
#include "arch/x86_64/allocation.h"

namespace toycc::arch::x86_64 {
    inline OperandMatch operator& (const OperandMatch& left, const OperandMatch& right) {
        if (left.match == OperandMatch::KO)
            return left;
        else if (right.match == OperandMatch::KO)
            return right;
        else if (left.match == OperandMatch::REQUIRES_TRANSFER && left.location.has_value() && left.free)
            return left;
        else if (right.match == OperandMatch::REQUIRES_TRANSFER && right.location.has_value() && right.free)
            return right;
        else if (left.match == OperandMatch::REQUIRES_TRANSFER && left.location.has_value())
            return left;
        else if (right.match == OperandMatch::REQUIRES_TRANSFER && right.location.has_value())
            return right;
        else if (left.match == OperandMatch::REQUIRES_TRANSFER)
            return left;
        else if (right.match == OperandMatch::REQUIRES_TRANSFER)
            return right;
        else if (left.location.has_value())
            return left;
        else if (right.location.has_value())
            return right;
        else
            return left;
    }

    inline OperandMatch operator| (const OperandMatch& left, const OperandMatch& right) {
        if (left.match == OperandMatch::OK && left.location.has_value())
            return left;
        else if (right.match == OperandMatch::OK && right.location.has_value())
            return right;
        else if (left.match == OperandMatch::OK)
            return left;
        else if (right.match == OperandMatch::OK)
            return right;
        else if (left.match == OperandMatch::REQUIRES_TRANSFER && left.location.has_value() && left.free)
            return left;
        else if (right.match == OperandMatch::REQUIRES_TRANSFER && right.location.has_value() && right.free)
            return right;
        else if (left.match == OperandMatch::REQUIRES_TRANSFER && left.location.has_value())
            return left;
        else if (right.match == OperandMatch::REQUIRES_TRANSFER && right.location.has_value())
            return right;
        else if (left.match == OperandMatch::REQUIRES_TRANSFER)
            return left;
        else if (right.match == OperandMatch::REQUIRES_TRANSFER)
            return right;
        else
            return left;
    }

    inline OperandMatch is_constant(const ir::Operand& operand) {
        return operand.is_constant() ? OperandMatch {OperandMatch::OK, Location::constant} : OperandMatch::KO;
    }

    inline OperandMatch is_variable(const ir::Operand& operand) {
        return operand.is_variable() ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch is_label(const ir::Operand& operand) {
        return operand.is_label() ? OperandMatch {OperandMatch::OK, Location::constant} : OperandMatch::KO;
    }

    inline OperandMatch is_dereference(const ir::Operand& operand) {
        return operand.is_dereference() ? OperandMatch {OperandMatch::OK, Location::memory} : OperandMatch::KO;
    }

    inline OperandMatch check_type(const ir::Operand& operand, ir::TypeCategory expected_category) {
        return (operand.type()->category == expected_category) ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch check_location(const StackFrame& frame, const ir::Operand& operand, Location expected_location) {
        const std::unordered_set<Location> locations = frame.locate(operand);
        if (locations.contains(expected_location))
            return {OperandMatch::OK, expected_location};
        else
            return {OperandMatch::REQUIRES_TRANSFER, expected_location, frame.content(expected_location).get() == nullptr};
    }

    inline OperandMatch check_size(const ir::Operand& operand, size_t expected_size) {
        return (operand.type()->size({}) == expected_size) ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch check_value_eq(const ir::Operand& operand, ir::IntegerConstant value) {
        return (operand.is_constant() && operand.constant().is_integer() && operand.constant().integer() == value) ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch check_value_ge(const ir::Operand& operand, ir::IntegerConstant value) {
        return (operand.is_constant() && operand.constant().is_integer() && operand.constant().integer() >= value) ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch check_value_le(const ir::Operand& operand, ir::IntegerConstant value) {
        return (operand.is_constant() && operand.constant().is_integer() && operand.constant().integer() <= value) ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch check_storage(const ir::Operand& operand, ir::StorageClass storage) {
        return (operand.is_variable() && (operand.declaration()->storage & storage)) ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch check_overwrite(const StackFrame& frame, const ir::DependencyGraph& graph, const ir::Operand& input_operand, const ir::Operand& output_operand, const GroupMatch& group_match) {
        if (input_operand.is_constant() || input_operand.is_label())
            return OperandMatch::REQUIRES_TRANSFER;  // Can't overwrite a constant
        if (output_operand.is_constant() || output_operand.is_label())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "A constant or label can't be an output", input_operand.location);

        if (input_operand.is_dereference() && output_operand.is_dereference())
            return (input_operand == output_operand) ? OperandMatch::OK : OperandMatch::REQUIRES_TRANSFER;

        if (input_operand.is_dereference() || output_operand.is_dereference())
            return OperandMatch::REQUIRES_TRANSFER;

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

        size_t remaining_liveness = 0;
        for (const ir::DependencyGraph::Edge& edge : graph.out_edges(input_node))
            if (!group_statements.contains(edge.exit) || (edge.attr.type & ir::DependencyType::LIVE_ON_EXIT))
                remaining_liveness += 1;

        const std::unordered_set<Location> locations = frame.locate(input_variable);
        if (locations.size() > remaining_liveness)
            return OperandMatch::OK;
        else
            return OperandMatch::REQUIRES_TRANSFER;
    }
}
