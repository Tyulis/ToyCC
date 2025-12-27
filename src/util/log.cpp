#include <iostream>
#include "util/log.h"

namespace toycc {
    void StandardLogger::operator() (Diagnostic diagnostic) {
        std::cerr << diagnostic << std::endl;
    }

    StandardLogger default_logger;
    Logger& log = default_logger;
}
