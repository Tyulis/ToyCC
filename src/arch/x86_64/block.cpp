#include "config.h"
#include "diagnostic.h"
#include "ir/declaration.h"
#include "ir/flow.h"
#include "arch/x86_64/codegen.h"
#include "arch/x86_64/execmodel.h"
#include "gen/execmodel/x86_64/group_matcher.h"
#include "gen/execmodel/x86_64/translation_matcher.h"
#include "gen/execmodel/x86_64/emission.h"
#include "util/strings.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_basic_block(StackFrame& frame, std::shared_ptr<ir::BasicBlock> block, const std::unordered_set<std::shared_ptr<ir::Declaration>>&) {
        if (block->label.has_value() && block->label->type != ir::LabelType::FUNCTION)
            frame.label(block->label->name);

        ir::DependencyGraph graph = block->dependencies;
        ir::DependencyMatrix matrix = to_dependency_matrix(graph);
        std::vector<GroupMatch> group_matches = toycc::execmodel::x86_64::match_groups(matrix);

        if (toycc::config::debug::with_translation_trace) {
            std::cerr << "New basic block :\n";
            std::cerr << "    Dependency graph :\n";
            std::cerr << indent(ir::dot_graph(block->dependencies, "block"), true, "        ") << "\n";
            std::cerr << "    Dependency matrix :\n";
            std::cerr << indent(dump(matrix), true, "        ") << "\n";
            std::cerr << "    Group matches :\n";
            for (const GroupMatch& match : group_matches)
                std::cerr << "        " << match << "\n";
        }

        while (!graph.empty()) {
            if (toycc::config::debug::with_translation_trace) {
                std::cerr << "    New iteration :\n";
                std::cerr << "        Dependency graph :\n";
                std::cerr << indent(ir::dot_graph(graph, "remainder"), true, "            ") << "\n";
                std::cerr << "        Stack frame :\n";
                std::cerr << indent(frame.dump(), true, "            ") << "\n";
            }

            try {
                code_generation_iteration(frame, graph, group_matches);
                clear_obsolete_matches(group_matches, graph);
            } catch (Diagnostic& diagnostic) {
                if (diagnostic.level() == DiagnosticLevel::INTERNAL_ERROR || diagnostic.level() == DiagnosticLevel::NOT_IMPLEMENTED) {
                    diagnostic.add_note(DiagnosticLevel::NOTE, frame.dump()).add_note(DiagnosticLevel::NOTE, ir::dot_graph(graph, "remainder"));
                    for (const GroupMatch& match : group_matches)
                        diagnostic.add_note(DiagnosticLevel::NOTE, dump(match));
                }
                throw diagnostic;
            }
        }
    }

    void CodeGenerator::code_generation_iteration(StackFrame& frame, ir::DependencyGraph& graph, const std::vector<GroupMatch>& group_matches) {
        std::vector<GroupMatch> entry_matches = find_entry_matches(graph, group_matches);
        if (entry_matches.empty())
            Diagnostic diagnostic(DiagnosticLevel::INTERNAL_ERROR, "No entry group matches");

        std::vector<TranslationMatch> translation_matches = toycc::execmodel::x86_64::match_translations(frame, graph, entry_matches);
        if (translation_matches.empty())
            Diagnostic diagnostic(DiagnosticLevel::INTERNAL_ERROR, "No translation match");

        TranslationMatch selected_match = select_translation(translation_matches);
        if (toycc::config::debug::with_comment_trace)
            frame.comment(dump(selected_match));

        try {
            emit_transfers(frame, selected_match);
            flush_indirects(frame, graph, selected_match);
            toycc::execmodel::x86_64::emit_code(frame, selected_match);
            clear_processed_statements(frame, graph, selected_match.group_match);
            frame.flush_intermediates();
        } catch (Diagnostic& diagnostic) {
            if (diagnostic.level() == DiagnosticLevel::INTERNAL_ERROR || diagnostic.level() == DiagnosticLevel::NOT_IMPLEMENTED)
                diagnostic.add_note(DiagnosticLevel::NOTE, dump(selected_match));
            throw diagnostic;
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

        if (toycc::config::debug::with_translation_trace) {
            std::cerr << "        Entry matches :\n";
            for (const GroupMatch& match : entry_matches)
                std::cerr << "            " << match << "\n";
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

        if (toycc::config::debug::with_translation_trace) {
            std::cerr << "        Selected translation :\n";
            std::cerr << indent(dump(matches.at(selected_index.value())), true, "            ") << "\n";
        }

        return matches.at(selected_index.value());
    }

    void CodeGenerator::clear_processed_statements(StackFrame& frame, ir::DependencyGraph& graph, const GroupMatch& match) {
        for (std::shared_ptr<ir::DependencyNode> statement : match.statements) {
            const ir::DependencyGraph::NodeSet connected_values = graph.connected_nodes(statement);
            const ir::DependencyGraph::NodeSet sinks = graph.sinks();

            // Flush outputs that are live on exit to the stack
            for (const ir::DependencyGraph::Edge& edge : graph.out_edges(statement)) {
                if (edge.attr.type & ir::DependencyType::LIVE_ON_EXIT) {
                    std::shared_ptr<ir::Declaration> variable = edge.exit->declaration();
                    if (variable->storage & ir::StorageClass::GLOBAL)
                        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Flushing global variable outputs is not supported", statement->statement().location);

                    if (!frame.locate(variable).contains(Location::stack))
                        transfer(frame, edge.exit->declaration(), Location::stack);
                }
            }

            graph.pop_node(statement);

            // Only free values that are completely disconnected.
            // For instance, if one is both an input and an output and the input gets disconnected, it shouldn't get freed because it's still valid as an output.
            // Logically that's (variables - connected variables)
            std::unordered_set<std::shared_ptr<ir::Declaration>> free_variables;
            for (std::shared_ptr<ir::DependencyNode> value : connected_values)
                free_variables.insert(value->declaration());

            for (std::shared_ptr<ir::DependencyNode> value : connected_values)
                if (graph.is_connected(value))
                    free_variables.erase(value->declaration());

            // Remove value nodes that aren't connected to the remaining statements
            for (std::shared_ptr<ir::DependencyNode> value : connected_values) {
                if (!graph.is_connected(value))
                    graph.pop_node(value);
                if (free_variables.contains(value->declaration()))
                    frame.free(value->declaration());
            }
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
