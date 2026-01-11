#include "arch/x86_64/codegen.h"
#include "gen/execmodel/x86_64/matcher.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_basic_block(StackFrame& frame, std::shared_ptr<BasicBlock> block, const std::unordered_set<std::shared_ptr<Declaration>>& globals) {
        DependencyGraph graph = block->dependencies;
        while (!graph.empty())
            generate_iteration(frame, graph, block, globals);
    }

    void CodeGenerator::generate_iteration(StackFrame& frame, DependencyGraph& graph, std::shared_ptr<BasicBlock> block, const std::unordered_set<std::shared_ptr<Declaration>>& globals) {
        std::vector<DependencyGraph> subgraphs = entry_statement_subgraphs(graph, execmodel::x86_64::MAX_TRANSLATION_DEPTH);
    }

    std::vector<DependencyGraph> CodeGenerator::entry_statement_subgraphs(const DependencyGraph& graph, size_t max_depth) const {

    }
}
