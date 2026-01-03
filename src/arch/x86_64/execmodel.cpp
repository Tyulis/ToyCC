#include "arch/x86_64/execmodel.h"

namespace toycc::arch::x86_64 {
    using toycc::ir::StatementTag;

    const std::unordered_map<ir::StatementTag, OperandSpec> OPERAND_SPECS = {
        {StatementTag::RETURN, {.lvalue_input = {}, .inputs = {ACCUMULATOR}, .output = {}}},
    };

    const std::unordered_map<LOC, std::unordered_map<size_t, std::string>> REGISTER_NAMES= {
        {LOC::A,    {{1,   "%al"}, {2,   "%ax"}, {4,  "%eax"}, {8, "%rax"}}},
        {LOC::B,    {{1,   "%bl"}, {2,   "%bx"}, {4,  "%ebx"}, {8, "%rbx"}}},
        {LOC::C,    {{1,   "%cl"}, {2,   "%cx"}, {4,  "%ecx"}, {8, "%rcx"}}},
        {LOC::D,    {{1,   "%dl"}, {2,   "%dx"}, {4,  "%edx"}, {8, "%rdx"}}},
        {LOC::SI,   {{1,  "%sil"}, {2,   "%si"}, {4,  "%esi"}, {8, "%rsi"}}},
        {LOC::DI,   {{1,  "%dil"}, {2,   "%di"}, {4,  "%edi"}, {8, "%rdi"}}},
        {LOC::SP,   {{1,  "%spl"}, {2,   "%sp"}, {4,  "%esp"}, {8, "%rsp"}}},
        {LOC::BP,   {{1,  "%bpl"}, {2,   "%bp"}, {4,  "%ebp"}, {8, "%rbp"}}},
        {LOC::R8,   {{1,  "%r8b"}, {2,  "%r8w"}, {4,  "%r8d"}, {8,  "%r8"}}},
        {LOC::R9,   {{1,  "%r9b"}, {2,  "%r9w"}, {4,  "%r9d"}, {8,  "%r9"}}},
        {LOC::R10,  {{1, "%r10b"}, {2, "%r10w"}, {4, "%r10d"}, {8, "%r10"}}},
        {LOC::R11,  {{1, "%r11b"}, {2, "%r11w"}, {4, "%r11d"}, {8, "%r11"}}},
        {LOC::R12,  {{1, "%r12b"}, {2, "%r12w"}, {4, "%r12d"}, {8, "%r12"}}},
        {LOC::R13,  {{1, "%r13b"}, {2, "%r13w"}, {4, "%r13d"}, {8, "%r13"}}},
        {LOC::R14,  {{1, "%r14b"}, {2, "%r14w"}, {4, "%r14d"}, {8, "%r14"}}},
        {LOC::R15,  {{1, "%r15b"}, {2, "%r15w"}, {4, "%r15d"}, {8, "%r15"}}},
    };
}
