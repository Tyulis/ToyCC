#include "diagnostic.h"
#include "ir/flow.h"
#include "arch/x86_64/codegen.h"
#include "arch/x86_64/execmodel.h"
#include "gen/execmodel/x86_64/group_matcher.h"
#include "gen/execmodel/x86_64/translation_matcher.h"
#include "gen/execmodel/x86_64/emission.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_basic_block(StackFrame& frame, std::shared_ptr<ir::BasicBlock> block, const std::unordered_set<std::shared_ptr<ir::Declaration>>&) {
        ir::DependencyGraph graph = block->dependencies;
        ir::DependencyMatrix matrix = to_dependency_matrix(graph);
        std::vector<GroupMatch> group_matches = toycc::execmodel::x86_64::match_groups(matrix);

        while (!graph.empty()) {
            code_generation_iteration(frame, graph, group_matches);
            clear_obsolete_matches(group_matches, graph);
        }
    }

    void CodeGenerator::code_generation_iteration(StackFrame& frame, ir::DependencyGraph& graph, const std::vector<GroupMatch>& group_matches) {
        std::vector<GroupMatch> entry_matches = find_entry_matches(graph, group_matches);
        std::vector<TranslationMatch> translation_matches = toycc::execmodel::x86_64::match_translations(frame, graph, entry_matches);
        if (translation_matches.size() == 0)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "No translation match");

        const TranslationMatch& selected_match = select_translation(translation_matches);
        if (selected_match.matches()) {
            toycc::execmodel::x86_64::emit_code(frame, selected_match);
            clear_processed_statements(graph, selected_match.group_match);
        } else {
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Transfers are not implemented");
        }
    }

    std::vector<GroupMatch> CodeGenerator::find_entry_matches(const ir::DependencyGraph& graph, const std::vector<GroupMatch>& group_matches) {
        std::vector<GroupMatch> entry_matches;
        for (const GroupMatch& match : group_matches) {
            bool is_entry_match = true;
            for (std::shared_ptr<ir::DependencyNode> statement : match.statements) {
                for (ir::DependencyGraph::Edge input : graph.in_edges(statement)) {
                    if (match.link_values.contains(input.entry))
                        continue;

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

    const TranslationMatch& CodeGenerator::select_translation(const std::vector<TranslationMatch>& matches) {
        std::optional<size_t> selected_index = {};
        for (const auto& [index, match] : std::ranges::enumerate_view(matches)) {
            if (!selected_index.has_value()) {
                selected_index = index;
                continue;
            }

            const TranslationMatch& previous_match = matches.at(selected_index.value());

            if (!previous_match.matches() && match.matches())
                selected_index = index;

            // Both match fully : take the largest group
            else if (previous_match.matches() && match.matches() && match.group_match.statements.size() > previous_match.group_match.statements.size())
                selected_index = index;

            // None match fully : take the translation that needs the fewest transfers
            else if (!previous_match.matches() && !match.matches() && match.nof_transfers() < previous_match.nof_transfers())
                selected_index = index;
        }

        if (!selected_index.has_value())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "No translation match selected");

        return matches.at(selected_index.value());
    }

    void CodeGenerator::clear_processed_statements(ir::DependencyGraph& graph, const GroupMatch& match) {
        for (std::shared_ptr<ir::DependencyNode> statement : match.statements)
            graph.pop_node(statement);

        // Remove value nodes that aren't connected to the remaining statements'
        for (std::shared_ptr<ir::DependencyNode> node : graph.nodes()) {
            if (!node->is_value())
                continue;

            if (!graph.is_connected(node))
                graph.pop_node(node);
        }
    }

    void CodeGenerator::clear_obsolete_matches(std::vector<GroupMatch>& group_matches, const ir::DependencyGraph& graph) {
        const std::vector<GroupMatch> staging = group_matches;
        group_matches.clear();
        for (const GroupMatch& match : staging) {
            bool keep_match = true;
            for (std::shared_ptr<ir::DependencyNode> statement : match.statements) {
                if (!graph.contains(statement)) {
                    keep_match = false;
                    break;
                }
            }

            if (keep_match)
                group_matches.push_back(match);
        }
    }
}
