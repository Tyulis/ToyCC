#include <subprocess.hpp>

#include "diagnostic.h"
#include "assembler.h"
#include "util/log.h"

namespace toycc {
    void assemble(std::string code, std::string object_file_name) {
        subprocess::CompletedProcess process = subprocess::run({"as", "-o", object_file_name},
                subprocess::RunBuilder().cin(code).cout(subprocess::PipeOption::pipe).cerr(subprocess::PipeOption::pipe));

        Diagnostic assembler_diagnostics(DiagnosticLevel::WARNING, process.cerr);
        if (process.returncode == 0) {
            if (!process.cerr.empty())
                log(assembler_diagnostics.level(DiagnosticLevel::WARNING));
        } else {
            throw assembler_diagnostics.level(DiagnosticLevel::ERROR);
        }
    }
}
