#pragma once

#include <array>
#include <vector>
#include <unordered_map>

#include "ir/statement.h"
#include "util/flags.hpp"

namespace toycc::arch::x86_64 {
    enum class LOC : std::size_t {NONE = 0x0000000000000000,
        A   = 0x00000001, B   = 0x00000002, C    = 0x00000004, D    = 0x00000008, SI   = 0x00000010, DI   = 0x00000020, BP   = 0x00000040, SP   = 0x00000080,
        R8  = 0x00000100, R9  = 0x00000200, R10  = 0x00000400, R11  = 0x00000800, R12  = 0x00001000, R13  = 0x00002000, R14  = 0x00004000, R15  = 0x00008000,
        MM0 = 0x00010000, MM1 = 0x00020000, MM2  = 0x00040000, MM3  = 0x00080000, MM4  = 0x00100000, MM5  = 0x00200000, MM6  = 0x00400000, MM7  = 0x00800000,
        MM8 = 0x01000000, MM9 = 0x02000000, MM10 = 0x04000000, MM11 = 0x08000000, MM12 = 0x10000000, MM13 = 0x20000000, MM14 = 0x40000000, MM15 = 0x80000000,

        // FIXME : Flags ? (PF, OF, SF, ZF, CF)
        CONSTANT = 0x1000000000000000, STATIC = 0x2000000000000000, STACK = 0x4000000000000000,
    };

    constexpr std::array<LOC, 6> INTEGER_REGISTER_ARGUMENTS = {LOC::DI,  LOC::SI,  LOC::D,   LOC::C,   LOC::R8,  LOC::R9};
    constexpr std::array<LOC, 8> FLOAT_REGISTER_ARGUMENTS   = {LOC::MM0, LOC::MM1, LOC::MM2, LOC::MM3, LOC::MM4, LOC::MM5, LOC::MM6, LOC::MM7};

    constexpr Flags<LOC> ACCUMULATOR      = LOC::A;
    constexpr Flags<LOC> BASE_REGISTERS   = LOC::A   | LOC::B   | LOC::C    | LOC::D;
    constexpr Flags<LOC> STRING_REGISTERS = LOC::SI  | LOC::DI;
    constexpr Flags<LOC> STACK_REGISTERS  = LOC::BP  | LOC::SP;
    constexpr Flags<LOC> EXT_REGISTERS    = LOC::R8  | LOC::R9  | LOC::R10  | LOC::R11  | LOC::R12  | LOC::R13  | LOC::R14  | LOC::R15;
    constexpr Flags<LOC> MM_REGISTERS     = LOC::MM0 | LOC::MM1 | LOC::MM2  | LOC::MM3  | LOC::MM4  | LOC::MM5  | LOC::MM6  | LOC::MM7  |
                                            LOC::MM8 | LOC::MM9 | LOC::MM10 | LOC::MM11 | LOC::MM12 | LOC::MM13 | LOC::MM14 | LOC::MM15;

    constexpr Flags<LOC> INTEGER_REGISTERS = BASE_REGISTERS | STRING_REGISTERS | EXT_REGISTERS;
    constexpr Flags<LOC> REGISTERS = INTEGER_REGISTERS | MM_REGISTERS;

    constexpr Flags<LOC> MEMORY = LOC::STATIC | LOC::STACK;
    constexpr Flags<LOC> ANY_OPERAND = INTEGER_REGISTERS | LOC::CONSTANT | MEMORY;

    constexpr Flags<LOC> CALLER_SAVED = LOC::A | LOC::C   | LOC::D   | LOC::SI  | LOC::DI | LOC::R8 | LOC::R9 | LOC::R10 | LOC::R11;
    constexpr Flags<LOC> CALLEE_SAVED = LOC::B | LOC::R12 | LOC::R13 | LOC::R14 | LOC::R15;
    // FIXME : XMM LOCs not implemented

    struct OperandSpec {
        Flags<LOC> lvalue_input;
        std::vector<Flags<LOC>> inputs;
        Flags<LOC> output;
    };

    extern const std::unordered_map<ir::StatementTag, OperandSpec> OPERAND_SPECS;
    extern const std::unordered_map<LOC, std::unordered_map<size_t, std::string>> REGISTER_NAMES;
}
