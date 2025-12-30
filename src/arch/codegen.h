#pragma once

#include <memory>
#include <iostream>

#include "ir/scope.h"

namespace toycc::arch {
    void generate(std::ostream& output, std::shared_ptr<ir::Scope> scope);
}
