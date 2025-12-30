#include "arch/x86_64/codegen.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::write_label(std::string name) {
        output->get() << name << ":\n";
    }

    void CodeGenerator::write_statement(std::string code) {
        output->get() << "\t" << code << "\n";
    }

    void CodeGenerator::write_directive(std::string code) {
        output->get() << "\t" << code << "\n";
    }
}
