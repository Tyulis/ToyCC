#include "diagnostic.h"
#include "ir/flow.h"
#include "arch/x86_64/codegen.h"
#include "arch/x86_64/execmodel.h"
#include "gen/execmodel/x86_64/group_matcher.h"
#include "gen/execmodel/x86_64/translation_matcher.h"
#include "gen/execmodel/x86_64/emission.h"
#include "gen/execmodel/x86_64/transfer_matcher.h"
#include "gen/execmodel/x86_64/transfer_emission.h"

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
        if (translation_matches.empty()) {
            Diagnostic diagnostic(DiagnosticLevel::INTERNAL_ERROR, "No translation match");
            for (const GroupMatch& group : entry_matches)
                diagnostic.add_note(DiagnosticLevel::NOTE, dump(group));
            throw diagnostic;
        }

        TranslationMatch selected_match = select_translation(translation_matches);
        if (!selected_match.matches())
            emit_transfers(frame, selected_match);

        toycc::execmodel::x86_64::emit_code(frame, selected_match);
        clear_processed_statements(graph, selected_match.group_match);
        frame.flush_intermediates();
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

        if (!selected_index.has_value()) {
            Diagnostic diagnostic(DiagnosticLevel::INTERNAL_ERROR, "No translation match selected");
            for (const TranslationMatch& match : matches)
                diagnostic.add_note(DiagnosticLevel::NOTE, dump(match));
            throw diagnostic;
        }

        return matches.at(selected_index.value());
    }

    void CodeGenerator::emit_transfers(StackFrame& frame, TranslationMatch& match) {
        for (const auto& [statement_index, statement_match] : std::ranges::enumerate_view(match.statements)) {
            ir::Statement& statement = match.group_match.statements[statement_index]->statement();
            for (const auto& [input_index, input_match] : std::ranges::enumerate_view(statement_match.input)) {
                if (input_match.match != OperandMatch::REQUIRES_TRANSFER)
                    continue;

                if (!input_match.location.has_value())
                    throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Transfers without location hints are not implemented").add_note(DiagnosticLevel::NOTE, dump(match));
                if (!input_match.free)
                    throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Transfers to occupied locations are not implemented").add_note(DiagnosticLevel::NOTE, dump(match));

                ir::Operand& input = statement.inputs[input_index];
                transfer(frame, input, input_match.location.value());
            }

            if (statement_match.output.has_value() && statement_match.output->match == OperandMatch::REQUIRES_TRANSFER)
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Output operand transfers are not implemented").add_note(DiagnosticLevel::NOTE, dump(match));
        }

        for (const auto& [allocation_index, allocation_match] : std::ranges::enumerate_view(match.allocations)) {
            if (allocation_match.match != OperandMatch::REQUIRES_TRANSFER)
                continue;
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Intermediate allocation transfers are not implemented");
        }
    }

    void CodeGenerator::transfer(StackFrame& frame, ir::Operand& operand, Location destination) {
        if (frame.locate(operand).empty())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Attempted a transfer on operand {} that has no location", operand.ir_code()), operand.location)
                   .add_note(DiagnosticLevel::NOTE, frame.dump());

        std::optional<TransferMatch> match = toycc::execmodel::x86_64::match_transfers(frame, operand, destination);
        if (!match.has_value())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("No transfer match for operand {}", operand.ir_code()), operand.location)
                   .add_note(DiagnosticLevel::NOTE, frame.dump());
        toycc::execmodel::x86_64::emit_transfer(frame, operand, destination, match.value());
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
