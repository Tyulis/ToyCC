#include <format>
#include <unordered_map>

#include "arch/datamodel.h"
#include "diagnostic.h"
#include "ir/declaration.h"
#include "arch/x86_64/assembly.h"

namespace toycc::arch::x86_64 {
    using namespace toycc::execmodel::x86_64;

    const std::unordered_map<Location, std::unordered_map<size_t, std::string>> REGISTER_NAMES = {
        {Location::a,    {{1,   "%al"}, {2,   "%ax"}, {4,  "%eax"}, {8, "%rax"}}},
        {Location::b,    {{1,   "%bl"}, {2,   "%bx"}, {4,  "%ebx"}, {8, "%rbx"}}},
        {Location::c,    {{1,   "%cl"}, {2,   "%cx"}, {4,  "%ecx"}, {8, "%rcx"}}},
        {Location::d,    {{1,   "%dl"}, {2,   "%dx"}, {4,  "%edx"}, {8, "%rdx"}}},
        {Location::si,   {{1,  "%sil"}, {2,   "%si"}, {4,  "%esi"}, {8, "%rsi"}}},
        {Location::di,   {{1,  "%dil"}, {2,   "%di"}, {4,  "%edi"}, {8, "%rdi"}}},
        {Location::sp,   {{1,  "%spl"}, {2,   "%sp"}, {4,  "%esp"}, {8, "%rsp"}}},
        {Location::bp,   {{1,  "%bpl"}, {2,   "%bp"}, {4,  "%ebp"}, {8, "%rbp"}}},
        {Location::r8,   {{1,  "%r8b"}, {2,  "%r8w"}, {4,  "%r8d"}, {8,  "%r8"}}},
        {Location::r9,   {{1,  "%r9b"}, {2,  "%r9w"}, {4,  "%r9d"}, {8,  "%r9"}}},
        {Location::r10,  {{1, "%r10b"}, {2, "%r10w"}, {4, "%r10d"}, {8, "%r10"}}},
        {Location::r11,  {{1, "%r11b"}, {2, "%r11w"}, {4, "%r11d"}, {8, "%r11"}}},
        {Location::r12,  {{1, "%r12b"}, {2, "%r12w"}, {4, "%r12d"}, {8, "%r12"}}},
        {Location::r13,  {{1, "%r13b"}, {2, "%r13w"}, {4, "%r13d"}, {8, "%r13"}}},
        {Location::r14,  {{1, "%r14b"}, {2, "%r14w"}, {4, "%r14d"}, {8, "%r14"}}},
        {Location::r15,  {{1, "%r15b"}, {2, "%r15w"}, {4, "%r15d"}, {8, "%r15"}}},
    };

    ssize_t stack_offset(StackFrame& frame, std::shared_ptr<ir::Declaration> variable) {
        return -static_cast<ssize_t>(frame.offset(variable) + variable->type->size(variable->location));
    }

    // -------- Intermediate allocation overload
    std::string emit_operand(Location location, size_t size) {
        switch (location) {
            case Location::constant:
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Constants should be handled elsewhere");

            case Location::a:
            case Location::b:
            case Location::c:
            case Location::d:
            case Location::sp:
            case Location::bp:
            case Location::si:
            case Location::di:
            case Location::r8:
            case Location::r9:
            case Location::r10:
            case Location::r11:
            case Location::r12:
            case Location::r13:
            case Location::r14:
            case Location::r15:
            case Location::mm0:
            case Location::mm1:
            case Location::mm2:
            case Location::mm3:
            case Location::mm4:
            case Location::mm5:
            case Location::mm6:
            case Location::mm7:
            case Location::mm8:
            case Location::mm9:
            case Location::mm10:
            case Location::mm11:
            case Location::mm12:
            case Location::mm13:
            case Location::mm14:
            case Location::mm15:
                return REGISTER_NAMES.at(location).at(size);

            case Location::stack:
            case Location::memory:
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Intermediate allocations can't be in memory");
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown location");
    }

    // -------- IR operand overload
    std::string location_code(StackFrame& frame, std::shared_ptr<ir::Declaration> variable, Location location) {
        switch (location) {
            case Location::constant:
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Constants should be handled elsewhere");

            case Location::a:
            case Location::b:
            case Location::c:
            case Location::d:
            case Location::si:
            case Location::di:
            case Location::r8:
            case Location::r9:
            case Location::r10:
            case Location::r11:
            case Location::r12:
            case Location::r13:
            case Location::r14:
            case Location::r15:
            case Location::mm0:
            case Location::mm1:
            case Location::mm2:
            case Location::mm3:
            case Location::mm4:
            case Location::mm5:
            case Location::mm6:
            case Location::mm7:
            case Location::mm8:
            case Location::mm9:
            case Location::mm10:
            case Location::mm11:
            case Location::mm12:
            case Location::mm13:
            case Location::mm14:
            case Location::mm15:
                switch (variable->type->storage_category()) {
                    case ir::TypeCategory::ARRAY:
                    case ir::TypeCategory::STRUCT:
                        return REGISTER_NAMES.at(location).at(DATAMODEL->pointer_size);
                    default:
                        return REGISTER_NAMES.at(location).at(variable->type->size({}));
                }

            case Location::stack:
                return std::format("{}(%rbp)", stack_offset(frame, variable));

            case Location::memory:
                return std::format("{}(%rip)", variable->name);

            case Location::sp:
            case Location::bp:
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Can't use SP or BP as a variable location", variable->location);
        }
        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown location", variable->location);
    }

    std::string size_suffix(std::shared_ptr<ir::Declaration> variable) {
        switch (variable->type->size({})) {
            case 1: return "b";
            case 2: return "w";
            case 4: return "l";
            case 8: return "q";
            default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("No size suffix for size {}", variable->type->size({})), variable->location);
        }
    }

    std::string emit_operand(StackFrame& frame, const ir::Operand& operand, Location location) {
        std::stringstream code;
        if (operand.is_dereference()) {
            ir::IntegerConstant offset = operand.indices[0].constant().integer();
            if (offset != 0)
                code << offset;
            code << "(";

            const std::unordered_set<Location> pointer_locations = frame.locate(operand.declaration());
            if (pointer_locations.empty())
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Pointer `{}` has no location", operand.pointer().ir_code()), operand.location);

            for (Location pointer_location : pointer_locations)
                if (pointer_location != Location::memory && pointer_location != Location::stack)
                    location = pointer_location;

            if (location == Location::memory || location == Location::stack)
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Can't emit dereference operand with pointer `{}` in memory", operand.pointer().ir_code()), operand.location);
        }

        if (operand.has_constant_base()) {
            const ir::Constant& base = operand.constant();
            switch (base.tag()) {
                case ir::Constant::INTEGER:  code << "$" << base.integer();  break;
                default: throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Non-integer constants are not implemented", base.location);
            }
        } else if (operand.has_label_base()) {
            code << operand.label();
        } else if (operand.has_variable_base()) {
            std::shared_ptr<ir::Declaration> variable = operand.declaration();
            code << location_code(frame, variable, location);
        } else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown operand type", operand.location);

        if (operand.is_dereference())
            code << ")";

        return code.str();
    }

    void move_operand(StackFrame& frame, const ir::Operand& operand, Location to) {
        if (operand.is_variable())
            frame.move(operand.declaration(), to);
    }
}
