#pragma once

#include <map>
#include <string>

#include "ir/scope.h"
#include "ir/declaration.h"
#include "util/flags.hpp"

namespace toycc::ir {
    enum class FunctionSpecifier {
        INLINE = 0x01, NORETURN = 0x02,
    };

    struct Function : public Declaration {
        Flags<FunctionSpecifier> specifiers;
        std::map<std::string, std::shared_ptr<Identifier>> parameters;
        Scope scope;
    };
}
