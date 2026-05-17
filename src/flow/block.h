#pragma once

#include <memory>
#include <armadillo>
#include <unordered_map>

#include "code_location.h"
#include "flow/dependencies.h"
#include "ir/declaration.h"
#include "ir/label.h"
#include "ir/statement.h"

namespace toycc::flow {
    enum class BasicBlockType {
        ENTRY, INNER, EXIT,
    };

    struct BasicBlock {
        public:
            BasicBlockType type;
            std::optional<ir::Label> label;
            DependencyGraph dependencies;
            std::shared_ptr<DependencyNode> exit_statement = nullptr;

            BasicBlock(BasicBlockType type, std::shared_ptr<size_t> unique_id, std::optional<ir::Label> label = {});

            std::string dot_subgraph(std::stringstream& dot, std::string cluster_name) const;

            void add_statement(const ir::Statement& statement, const std::unordered_set<std::shared_ptr<ir::Declaration>>& defined_decls);
            void finish();
            void not_live_on_exit(const std::unordered_set<std::shared_ptr<ir::Declaration>>& intermediate);

            std::unordered_set<std::shared_ptr<ir::Declaration>> locals() const;
            std::unordered_set<std::shared_ptr<ir::Declaration>> live_on_entry() const;
            std::unordered_set<std::shared_ptr<ir::Declaration>> live_on_exit() const;

            ConstantMap output_constants() const;
            bool has_calls() const;

            // NOTE : Optimization passes have all levels (unit, procedure, block) in a single separate file
            void opt_constant_folding(ConstantMap initial_constants);  // -> flow/optimization/constant_folding.cpp
            void opt_split_intermediates();                            // -> flow/optimization/split_intermediates.cpp

        private:
            std::shared_ptr<size_t> unique_id;
            std::unordered_map<std::shared_ptr<ir::Declaration>, std::shared_ptr<DependencyNode>> last_modification;

            std::shared_ptr<ir::Declaration> declare_intermediate(std::shared_ptr<ir::Type> type, CodeLocation location);
    };

}
