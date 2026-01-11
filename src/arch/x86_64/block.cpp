#include "arch/x86_64/execmodel.h"
#include "ir/flow.h"
#include "arch/x86_64/codegen.h"
#include "gen/execmodel/x86_64/matcher.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_basic_block(StackFrame& frame, std::shared_ptr<BasicBlock> block, const std::unordered_set<std::shared_ptr<Declaration>>&) {
        DependencyGraph graph = block->dependencies;
        DependencyMatrix matrix = to_dependency_matrix(graph);
        std::vector<GroupMatch> group_matches = toycc::execmodel::x86_64::match_groups(matrix);

        while (!graph.empty()) {
            code_generation_iteration(frame, graph, group_matches);
            clear_obsolete_matches(group_matches, graph);
        }
    }

    void CodeGenerator::code_generation_iteration(StackFrame& frame, DependencyGraph& graph, const std::vector<GroupMatch>& group_matches) {
        std::vector<GroupMatch> entry_matches = find_entry_matches(graph, group_matches);
        std::vector<TranslationMatch> translation_matches = match_translations();
    }

    void CodeGenerator::clear_obsolete_matches(std::vector<GroupMatch>& group_matches, const DependencyGraph& graph) {
        const std::vector<GroupMatch> staging = group_matches;
        group_matches.clear();
        for (const GroupMatch& match : staging) {
            bool keep_match = true;
            for (std::shared_ptr<DependencyNode> statement : match.statements) {
                if (!graph.contains(statement)) {
                    keep_match = false;
                    break;
                }
            }

            if (keep_match)
                group_matches.push_back(match);
        }
    }

    std::vector<GroupMatch> CodeGenerator::find_entry_matches(const DependencyGraph& graph, const std::vector<GroupMatch>& group_matches) {
        std::vector<GroupMatch> entry_matches;
        for (const GroupMatch& match : group_matches) {
            bool is_entry_match = true;
            for (std::shared_ptr<DependencyNode> statement : match.statements) {
                for (DependencyGraph::Edge input : graph.in_edges(statement)) {
                    if (!graph.is_source(input.entry)) {
                        is_entry_match = false;
                        goto exit_statement_loop;
                    }
                }
            }
            exit_statement_loop:;

            if (is_entry_match)
                entry_matches.push_back(match);
        }

        return entry_matches;
    }
}
