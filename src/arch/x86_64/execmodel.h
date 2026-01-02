#pragma once

#include <array>

namespace toycc::arch::x86_64 {
    enum class Register {
        A, B, C, D, SI, DI, R8, R9, R10, R11, R12, R13, R14, R15,
        MM0, MM1, MM2, MM3, MM4, MM5, MM6, MM7, MM8, MM9, MM10, MM11, MM12, MM13, MM14, MM15,
    };

    constexpr std::array<Register, 6> INTEGER_REGISTER_ARGUMENTS = {Register::DI, Register::SI, Register::D, Register::C, Register::R8, Register::R9};
}
