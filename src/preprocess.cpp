#include <subprocess.hpp>

#include "diagnostic.h"
#include "preprocess.h"

namespace toycc {
    void preprocess(std::string filename, std::ostream& output) {
        subprocess::CompletedProcess process = subprocess::run({"cpp", "-std=c11", filename},
                subprocess::RunBuilder().cout(subprocess::PipeOption::pipe).cerr(subprocess::PipeOption::pipe));

        output << process.cout;

        Diagnostic preprocessor_diagnostics(Diagnostic::Level::WARNING, process.cerr, filename);
        if (process.returncode == 0) {
            if (!process.cerr.empty())
                std::cerr << preprocessor_diagnostics.level(Diagnostic::Level::WARNING).message() << std::endl;
        } else {
            throw preprocessor_diagnostics.level(Diagnostic::Level::ERROR);
        }
    }
}
