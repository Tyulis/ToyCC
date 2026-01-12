#pragma once

#include "ir/type.h"
#include "ir/declaration.h"
#include "arch/x86_64/execmodel.h"
#include "arch/x86_64/allocation.h"

namespace toycc::arch::x86_64 {
    inline OperandMatch operator& (const OperandMatch& left, const OperandMatch& right) {
        if (left == OperandMatch::KO || right == OperandMatch::KO)
            return OperandMatch::KO;
        else if (left == OperandMatch::REQUIRES_TRANSFER || right == OperandMatch::REQUIRES_TRANSFER)
            return OperandMatch::REQUIRES_TRANSFER;
        else return OperandMatch::OK;
    }

    inline OperandMatch operator| (const OperandMatch& left, const OperandMatch& right) {
        if (left == OperandMatch::OK || right == OperandMatch::OK)
            return OperandMatch::OK;
        else if (left == OperandMatch::REQUIRES_TRANSFER || right == OperandMatch::REQUIRES_TRANSFER)
            return OperandMatch::REQUIRES_TRANSFER;
        else return OperandMatch::KO;
    }

    inline OperandMatch is_constant(const ir::Operand& operand) {
        return operand.is_constant() ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch is_variable(const ir::Operand& operand) {
        return operand.is_variable() ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch is_label(const ir::Operand& operand) {
        return operand.is_label() ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch is_dereference(const ir::Operand& operand) {
        return operand.is_dereference() ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch check_type(const ir::Operand& operand, ir::TypeCategory expected_category) {
        return (operand.type()->category == expected_category) ? OperandMatch::OK : OperandMatch::KO;
    }

    inline OperandMatch check_location(const StackFrame& frame, const ir::Operand& operand, Location expected_location) {
        const std::unordered_set<Location> locations = frame.locate(operand);
        if (locations.contains(expected_location))
            return OperandMatch::OK;
        else
            return OperandMatch::REQUIRES_TRANSFER;
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
}
