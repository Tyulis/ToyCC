#include <subprocess.hpp>
#include "preprocess.h"

namespace toycc {
    std::string preprocess(std::string filename) {
        subprocess::CompletedProcess process = subprocess::run({"cpp", filename},
                subprocess::RunBuilder().cout(subprocess::PipeOption::pipe));

        return process.cout;
    }
}
