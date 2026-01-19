#include "config.h"
#include "diagnostic.h"
#include "ir/declaration.h"
#include "ir/flow.h"
#include "arch/x86_64/codegen.h"
#include "arch/x86_64/execmodel.h"
#include "gen/execmodel/x86_64/group_matcher.h"
#include "gen/execmodel/x86_64/translation_matcher.h"
#include "gen/execmodel/x86_64/emission.h"
#include "gen/execmodel/x86_64/transfer_matcher.h"
#include "gen/execmodel/x86_64/transfer_emission.h"
#include "util/strings.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_basic_block(StackFrame& frame, std::shared_ptr<ir::BasicBlock> block, const std::unordered_set<std::shared_ptr<ir::Declaration>>&) {
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
            load_pointers(frame, selected_match);
            if (!selected_match.matches())
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

    // Check that the pointers used in this translation are in registers, otherwise load them
    void CodeGenerator::load_pointers(StackFrame& frame, TranslationMatch& match) {
        auto load_pointer = [&](ir::Operand& operand) {
            // No pointer, or the pointer can't be in memory
            if (!operand.is_dereference() || !operand.has_variable_base())
                return;

            const std::unordered_set<Location> pointer_locations = frame.locate(operand.declaration());
            if (pointer_locations.empty())
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("The pointer in operand {} has no location", operand.ir_code()), operand.location);

            bool is_only_memory = (pointer_locations.contains(Location::memory) || pointer_locations.contains(Location::stack));
            for (Location location : pointer_locations)
                if (location != Location::memory && location != Location::stack)
                    is_only_memory = false;

            if (!is_only_memory)
                return;  // The pointer is already loaded

            // The pointer itself is in memory, so it needs to be transferred to registers before dereferencing it
            ir::Operand pointer = operand.pointer();
            Location destination = allocate_main_register(frame, match);
            transfer(frame, pointer, destination);
            operand.value = pointer.value;
        };

        for (std::shared_ptr<ir::DependencyNode> statement : match.group_match.statements) {
            for (ir::Operand& operand : statement->statement().inputs)
                load_pointer(operand);
            if (statement->statement().output.has_value())
                load_pointer(statement->statement().output.value());
        }
    }

    // Allocate a main register that isn't allocated to one of this match's other operands, in that order
    static const std::vector<Location> MAIN_REGISTERS = {Location::a, Location::b, Location::c, Location::d, Location::si, Location::di,
                                                         Location::r8, Location::r9, Location::r10, Location::r11, Location::r12, Location::r13, Location::r14, Location::r15};

    Location CodeGenerator::allocate_main_register(StackFrame& frame, TranslationMatch& match) {
        // Find all registers used in this translation match
        std::unordered_set<Location> allocated_locations;

        for (std::shared_ptr<ir::DependencyNode> statement_node : match.group_match.statements) {
            const ir::Statement& statement = statement_node->statement();
            for (const ir::Operand& input : statement.inputs)
                if (input.has_variable_base())
                    allocated_locations.insert_range(frame.locate(input.declaration()));
            if (statement.output.has_value())
                if (statement.output->has_variable_base())
                    allocated_locations.insert_range(frame.locate(statement.output->declaration()));
        }

        for (const StatementMatch& statement_match : match.statements) {
            for (const OperandMatch& input_match : statement_match.input)
                if (input_match.location.has_value())
                    allocated_locations.insert(input_match.location.value());

            if (statement_match.output.has_value() && statement_match.output->location.has_value())
                allocated_locations.insert(statement_match.output->location.value());
        }

        for (const OperandMatch& allocation : match.allocations)
            if (allocation.location.has_value())
                allocated_locations.insert(allocation.location.value());

        // First pass : try to find a register that's currently free and will stay free throughout the translation
        for (Location reg : MAIN_REGISTERS)
            if (!allocated_locations.contains(reg) && frame.is_free(reg))
                return reg;

        // No such free registers : we'll need to spill a variable to memory
        for (Location reg : MAIN_REGISTERS) {
            std::shared_ptr<ir::Declaration> content = frame.content(reg);
            if (!allocated_locations.contains(reg)) {
                // Found a variable that's not used in the current translation, spill it to memory
                if (content->storage & ir::StorageClass::GLOBAL)
                    throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Spilling global variables to memory is not implemented");

                ir::Operand operand(content, content->location);
                transfer(frame, operand, Location::stack);
                return reg;
            }
        }

        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "No variable outside of the current translation can be spilled");
    }

    // Before a dereference or call, flush all indirect operands to their respective memory locations
    void CodeGenerator::flush_indirects(StackFrame& frame, const ir::DependencyGraph& graph, const TranslationMatch& match) {
        std::unordered_set<std::shared_ptr<ir::Declaration>> indirects;
        std::unordered_set<std::shared_ptr<ir::Declaration>> reads;
        for (std::shared_ptr<ir::DependencyNode> statement : match.group_match.statements) {
            for (ir::DependencyGraph::Edge edge : graph.connected_edges(statement)) {
                std::shared_ptr<ir::DependencyNode> value = (edge.entry == statement ? edge.exit : edge.entry);
                std::shared_ptr<ir::Declaration> variable = value->declaration();

                if (edge.attr.operand_group == ir::OperandGroup::INDIRECT && (edge.attr.type & (ir::DependencyType::DEREFERENCE | ir::DependencyType::CALL | ir::DependencyType::LIVE_ON_EXIT)))
                    indirects.insert(variable);

                if (edge.attr.type & ir::DependencyType::READ)
                    reads.insert(variable);
            }
        }

        for (std::shared_ptr<ir::Declaration> variable : indirects) {
            const std::unordered_set<Location> locations = frame.locate(variable);

            Location destination = Location::stack;
            if (locations.contains(Location::memory) && !locations.contains(Location::stack))
                destination = Location::memory;

            // Move the variable to memory if necessary
            if (!locations.contains(destination)) {
                if (variable->storage & ir::StorageClass::GLOBAL)
                    throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Flushing indirect variables is not implemented", variable->location);

                ir::Operand operand(variable, variable->location);
                transfer(frame, operand, Location::stack);
                frame.copy(variable, Location::stack);
            }

            if (!reads.contains(variable))
                frame.move(variable, destination);  // Remove its possible other locations if it's not needed as an input
        }
    }

    void CodeGenerator::emit_transfers(StackFrame& frame, TranslationMatch& match) {
        for (const auto& [statement_index, statement_match] : std::ranges::enumerate_view(match.statements)) {
            ir::Statement& statement = match.group_match.statements[statement_index]->statement();
            for (const auto& [input_index, input_match] : std::ranges::enumerate_view(statement_match.input)) {
                if (input_match.match != OperandMatch::REQUIRES_TRANSFER)
                    continue;

                if (!input_match.location.has_value())
                    throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Transfers without location hints are not implemented");
                if (!input_match.free)
                    throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Transfers to occupied locations are not implemented");

                ir::Operand& input = statement.inputs[input_index];
                transfer(frame, input, input_match.location.value());
            }

            if (statement_match.output.has_value() && statement_match.output->match == OperandMatch::REQUIRES_TRANSFER)
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Output operand transfers are not implemented")
                       .add_note(DiagnosticLevel::NOTE, dump(match));
        }

        for (const auto& [allocation_index, allocation_match] : std::ranges::enumerate_view(match.allocations)) {
            if (allocation_match.match != OperandMatch::REQUIRES_TRANSFER)
                continue;
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Intermediate allocation transfers are not implemented");
        }
    }

    void CodeGenerator::transfer(StackFrame& frame, ir::Operand& operand, Location destination) {
        if (frame.locate(operand).empty())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Attempted a transfer on operand {} that has no location", operand.ir_code()), operand.location);

        std::optional<TransferMatch> match = toycc::execmodel::x86_64::match_transfers(frame, operand, destination);
        if (!match.has_value())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("No transfer match for operand {}", operand.ir_code()), operand.location);

        Location source = match->source_location.value_or(*frame.locate(operand).begin());
        ir::Operand source_operand = operand;
        if (operand.is_constant() || operand.is_dereference()) {
            std::shared_ptr<ir::Declaration> temporary = frame.declare_intermediate(operand.type(), operand.location);
            frame.copy(temporary, destination);
            operand = temporary;
        } else if (operand.is_label()) {
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Transferring labels is not supported", operand.location);
        } else if (operand.is_variable()) {
            frame.copy(operand.declaration(), destination);
        } else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown operand type", operand.location);

        if (toycc::config::debug::with_comment_trace) {
            std::stringstream comment;
            comment << "TRANSFER " << source_operand.ir_code() << "(" << source << ") -> " << operand.ir_code() << "(" << destination << ")";
            frame.comment(comment.str());
        }

        if (toycc::config::debug::with_translation_trace) {
            std::cerr << "        Transfer " << source_operand.ir_code() << "(" << source << ") -> " << operand.ir_code() << "(" << destination << ")" << "\n";
            std::cerr << indent(dump(match.value()), true, "            ") << "\n";
        }


        toycc::execmodel::x86_64::emit_transfer(frame, source_operand, operand, match.value(), source, destination);
    }

    void CodeGenerator::clear_processed_statements(StackFrame& frame, ir::DependencyGraph& graph, const GroupMatch& match) {
        for (std::shared_ptr<ir::DependencyNode> statement : match.statements) {
            ir::DependencyGraph::NodeSet connected_values = graph.connected_nodes(statement);
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
