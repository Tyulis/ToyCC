#pragma once

#include "flow/block.h"
#include "ir/declaration.h"
#include "ir/scope.h"
#include "ir/statement.h"
#include "util/graph.hpp"

namespace toycc::flow {
    enum class FlowType {
        FALLTHROUGH,  // In order in the original code, fall directly from one block to the next without jump
        JUMP,         // Transition via any jump instruction (including `return`)
    };

    std::ostream& operator<< (std::ostream& stream, FlowType type);

    using FlowGraph = Graph<BasicBlock, FlowType>;
    using GlobalMap = std::unordered_map<std::shared_ptr<ir::Declaration>, std::optional<ir::Constant>>;
    using FallthroughChain = std::vector<std::shared_ptr<BasicBlock>>;

    struct Procedure {
        public:
            std::shared_ptr<ir::Declaration> declaration;
            CodeLocation location;
            FlowGraph blocks;
            std::shared_ptr<BasicBlock> entry_block;
            std::shared_ptr<BasicBlock> exit_block;
            std::vector<std::shared_ptr<ir::Declaration>> parameters;

            Procedure() = default;
            Procedure(const ir::Statement& function, const GlobalMap& globals, std::shared_ptr<size_t> unique_id);

            std::string start_label() const;
            std::string end_label() const;

            std::string dot_subgraph(std::stringstream& dot) const;

            std::unordered_set<std::shared_ptr<ir::Declaration>> locals() const;
            std::unordered_set<std::shared_ptr<ir::Declaration>> live_through(std::shared_ptr<BasicBlock> block) const;
            std::unordered_set<std::shared_ptr<ir::Declaration>> live_on_entry(std::shared_ptr<BasicBlock> block) const;
            std::unordered_set<std::shared_ptr<ir::Declaration>> live_on_exit(std::shared_ptr<BasicBlock> block) const;
            std::vector<FallthroughChain> fallthrough_chains() const;

            bool is_leaf() const;

        private:
            std::shared_ptr<size_t> unique_id;

            void build_flow_graph(std::shared_ptr<ir::Scope> scope, const GlobalMap& globals);
            void resolve_intermediates();
    };
}
