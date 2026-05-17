#pragma once

#include <string>
#include <memory>
#include <unordered_map>

#include "flow/procedure.h"
#include "ir/scope.h"

namespace toycc::flow {
    struct TranslationUnit {
        std::string working_directory;
        std::string filename;
        GlobalMap globals;
        std::unordered_map<std::string, Procedure> procedures;
        std::shared_ptr<size_t> unique_id = 0;

        TranslationUnit() = default;
        TranslationUnit(std::shared_ptr<ir::Scope> global_scope, std::string working_directory, std::string filename);

        std::string dot_graph() const;
    };
}
