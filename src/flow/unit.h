#pragma once

#include <string>
#include <memory>
#include <unordered_map>

#include "flow/block.h"
#include "flow/procedure.h"
#include "ir/scope.h"

namespace toycc::flow {
    class TranslationUnit {
        public:
            std::string working_directory;
            std::string filename;
            std::unordered_map<std::string, Procedure> procedures;
            GlobalMap globals;

            TranslationUnit() = default;
            TranslationUnit(std::shared_ptr<ir::Scope> global_scope, std::string working_directory, std::string filename);

            std::string dot_graph() const;

        private:
            std::shared_ptr<size_t> unique_id = 0;
            std::shared_ptr<BasicBlock> global_block;
    };
}
