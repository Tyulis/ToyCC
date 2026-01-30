#pragma once

#include "arch/x86_64/execmodel.h"
#include "arch/x86_64/allocation.h"

namespace toycc::arch::x86_64 {
    void emit_addressof(StackFrame& frame, const TranslationMatch& match);
    void emit_call(StackFrame& frame, const TranslationMatch& match);
    void emit_return(StackFrame& frame, const TranslationMatch& match);
}
