#pragma once

#include <map>
#include <memory>
#include <vector>

#include "ir/type.h"
#include "ir/statement.h"
#include "ir/declaration.h"

namespace toycc::ir {
    struct Scope {
        std::shared_ptr<Declaration> function;

        std::map<ir::TypeIdentifier, std::shared_ptr<Type>> types;
        std::map<std::string, std::shared_ptr<Declaration>> typedefs;
        std::map<std::string, std::shared_ptr<Declaration>> locals;
        std::vector<std::shared_ptr<Statement>> statements;

        std::string ir_code() const;
    };
}
