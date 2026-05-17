#pragma once

#include "diagnostic.h"
#include "flow/block.h"
#include "ir/type.h"
#include "ir/declaration.h"
#include "arch/x86_64/execmodel.h"
#include "arch/x86_64/allocation.h"
#include "util/sets.hpp"

namespace toycc::arch::x86_64 {
    inline OperandMatch operator& (const OperandMatch& left, const OperandMatch& right) {
        if (left.match == OperandMatch::KO || right.match == OperandMatch::KO)
            return OperandMatch::KO;
        else if (left.match == OperandMatch::REQUIRES_TRANSFER || right.match == OperandMatch::REQUIRES_TRANSFER)
            return {OperandMatch::REQUIRES_TRANSFER, unordered_set_intersection(left.locations, right.locations)};
        else
            return {OperandMatch::OK, unordered_set_intersection(left.locations, right.locations)};
    }

    inline OperandMatch operator| (const OperandMatch& left, const OperandMatch& right) {
        if (left.match == OperandMatch::OK || right.match == OperandMatch::OK)
            return {OperandMatch::OK, unordered_set_union(left.locations, right.locations)};
        else if (left.match == OperandMatch::REQUIRES_TRANSFER || right.match == OperandMatch::REQUIRES_TRANSFER)
            return {OperandMatch::REQUIRES_TRANSFER, unordered_set_union(left.locations, right.locations)};
        else
            return OperandMatch::KO;
    }

    inline OperandMatch is_constant(const ir::Operand& operand) {
        return operand.is_constant() ? OperandMatch {OperandMatch::OK, {Location::constant}} : OperandMatch::KO;
    }

    inline OperandMatch is_variable(const ir::Operand& operand) {
        return operand.is_variable() ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch is_label(const ir::Operand& operand) {
        return operand.is_label() ? OperandMatch {OperandMatch::OK, {Location::constant}} : OperandMatch::KO;
    }

    inline OperandMatch is_dereference(const ir::Operand& operand) {
        return operand.is_dereference() ? OperandMatch {OperandMatch::OK, {Location::memory}} : OperandMatch::KO;
    }

    inline OperandMatch check_type(const ir::Operand& operand, ir::TypeCategory expected_category) {
        return (operand.type()->storage_category() == expected_category) ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch check_in_location(const StackFrame& frame, const ir::Operand& operand, Location expected_location) {
        const std::unordered_set<Location> locations = frame.locate(operand);
        if (locations.empty())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Input operand {} has no location", operand.ir_code()), operand.location);

        if (locations.contains(expected_location))
            return {OperandMatch::OK, {expected_location}};
        else
            return {OperandMatch::REQUIRES_TRANSFER, {expected_location}};
    }

    inline OperandMatch check_out_location(const StackFrame& frame, const ir::Operand& operand, Location expected_location) {
        const std::unordered_set<Location> locations = frame.locate(operand);

        // Global variables will need to be flushed back to memory shortly after, so consider non-memory locations for them as requiring transfers
        if (operand.is_variable()) {
            std::shared_ptr<ir::Declaration> variable = operand.declaration();
            if (variable->storage & ir::StorageClass::GLOBAL && expected_location != Location::memory)
                return {OperandMatch::REQUIRES_TRANSFER, {expected_location}};
        }

        // The output is already in the expected location -> trivial OK
        // If the expected location is free, then the result may go into it directly as an output operand so it's also OK,
        //     BUT this only applies to actual variables, dereferences are always in a fixed place in memory so the output location can't be chosen freely like that
        if (locations.contains(expected_location) || (operand.is_variable() && frame.is_free(expected_location)))
            return {OperandMatch::OK, {expected_location}};
        else
            return {OperandMatch::REQUIRES_TRANSFER, {expected_location}};
    }

    inline OperandMatch check_size(const ir::Operand& operand, size_t expected_size) {
        return (operand.type()->size({}) == expected_size) ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch check_value_eq(const ir::Operand& operand, ir::IntegerConstant value) {
        return (operand.is_constant() && operand.constant().tag() == ir::Constant::INTEGER && operand.constant().integer() == value) ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch check_value_ge(const ir::Operand& operand, ir::IntegerConstant value) {
        return (operand.is_constant() && operand.constant().tag() == ir::Constant::INTEGER && operand.constant().integer() >= value) ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch check_value_le(const ir::Operand& operand, ir::IntegerConstant value) {
        return (operand.is_constant() && operand.constant().tag() == ir::Constant::INTEGER && operand.constant().integer() <= value) ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch check_storage(const ir::Operand& operand, ir::StorageClass storage) {
        return (operand.is_variable() && (operand.declaration()->storage & storage)) ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch check_signed(const ir::Operand& operand, bool expect_signed) {
        std::shared_ptr<ir::Type> base_type = operand.type()->dequalify();
        if (base_type->storage_category() != ir::TypeCategory::INTEGER)
            return OperandMatch::KO;

        std::shared_ptr<ir::IntegerType> integer_type = std::static_pointer_cast<ir::IntegerType> (base_type);
        return (integer_type->is_signed == expect_signed ? OperandMatch::OK : OperandMatch::KO);
    }

    OperandMatch check_overwrite(const StackFrame& frame, const flow::DependencyGraph& graph, const ir::Operand& input_operand, const ir::Operand& output_operand, const GroupMatch& group_match);
    OperandMatch check_implicit_overwrite(const StackFrame& frame, const flow::DependencyGraph& graph, const ir::Operand& input_operand, const GroupMatch& group_match, Location overwritten_location);
}
