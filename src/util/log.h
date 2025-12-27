#pragma once

#include "diagnostic.h"

namespace toycc {
    class Logger {
        public:
            virtual void operator() (Diagnostic diagnostic) = 0;
    };

    class StandardLogger : public Logger {
        public:
            virtual void operator() (Diagnostic diagnostic) override;
    };

    extern StandardLogger default_logger;
    extern Logger& log;
}
