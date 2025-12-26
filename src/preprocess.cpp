#include <subprocess.hpp>

#include "diagnostic.h"
#include "preprocess.h"

namespace toycc {
    void preprocess(std::string filename, std::ostream& output) {
        subprocess::CompletedProcess process = subprocess::run({"cpp", "-std=c11", "-undef", "-include", "include/__toycc_prologue.h", "-include", "include/__toycc_prologue_x86_64.h", filename},
                subprocess::RunBuilder().cout(subprocess::PipeOption::pipe).cerr(subprocess::PipeOption::pipe));

        output << process.cout;

        Diagnostic preprocessor_diagnostics(DiagnosticLevel::WARNING, process.cerr, filename);
        if (process.returncode == 0) {
            if (!process.cerr.empty())
                std::cerr << preprocessor_diagnostics.level(DiagnosticLevel::WARNING).message() << std::endl;
        } else {
            throw preprocessor_diagnostics.level(DiagnosticLevel::ERROR);
        }
    }
}
