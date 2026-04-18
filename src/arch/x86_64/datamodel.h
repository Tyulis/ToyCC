#pragma once

#include "arch/datamodel.h"

namespace toycc::arch::x86_64 {
    struct DataModel : public toycc::arch::DataModel {
        DataModel();
    };

    extern DataModel DATAMODEL;
}
