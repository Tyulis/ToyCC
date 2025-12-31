#include <subprocess.hpp>

#include "diagnostic.h"
#include "linker.h"
#include "util/log.h"

namespace toycc {
    void link(std::string object_file_name, std::string output_file_name) {
        subprocess::CompletedProcess process = subprocess::run({"gcc", "-o", output_file_name, object_file_name},
                                                               subprocess::RunBuilder().cout(subprocess::PipeOption::pipe).cerr(subprocess::PipeOption::pipe));

        Diagnostic linker_diagnostics(DiagnosticLevel::WARNING, process.cerr);
        if (process.returncode == 0) {
            if (!process.cerr.empty())
                log(linker_diagnostics.level(DiagnosticLevel::WARNING));
        } else {
            throw linker_diagnostics.level(DiagnosticLevel::ERROR);
        }
    }
}
