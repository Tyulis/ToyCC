#include "diagnostic.h"
#include "flow/procedure.h"
#include "ir/type_expressions.h"

namespace toycc::flow {
    std::ostream& operator<< (std::ostream& stream, FlowType type) {
        switch (type) {
            case FlowType::FALLTHROUGH:  stream << "FALLTHROUGH";  break;
            case FlowType::JUMP:         stream << "JUMP";         break;
        }
        return stream;
    }

    // -------- Procedure
    Procedure::Procedure(const ir::Statement& function, const ConstantMap& globals, std::shared_ptr<size_t> unique_id)
    : declaration(function.output->declaration()), location(function.location), unique_id(unique_id)
    {
        if (function.tag != ir::StatementTag::FUNCTION)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to initialize a procedure with a statement that's not a function", function.location);

        std::shared_ptr<ir::Scope> scope = function.block;
        entry_block = blocks.emplace_node(BasicBlockType::ENTRY, unique_id);
        exit_block  = blocks.emplace_node(BasicBlockType::EXIT,  unique_id,
                                          ir::Label {.type = ir::LabelType::INTERNAL, .name = std::format(".L{}.BB.__exit", declaration->name), .location = function.location});

        for (std::shared_ptr<ir::Declaration> declaration : scope->locals_list())
            if (declaration->storage & ir::StorageClass::PARAMETER)
                parameters.push_back(declaration);

        build_flow_graph(scope, globals);
        resolve_intermediates();
    }

    // Write the graph in dot format to `dot`, return the name of any node in the cluster
    std::string Procedure::dot_subgraph(std::stringstream& dot) const {
        const std::string procedure_cluster = std::format("cluster_{}", declaration->name);
        dot << "subgraph " << procedure_cluster << " {\n";
        dot << "label = \"" << declaration->name << "\";\n";

        size_t block_index = 0;
        std::unordered_map<std::shared_ptr<BasicBlock>, std::string> cluster_names;
        std::unordered_map<std::shared_ptr<BasicBlock>, std::string> block_nodes;
        for (std::shared_ptr<BasicBlock> block : blocks.nodes()) {
            const std::string cluster_name = std::format("{}_{}", procedure_cluster, block_index++);
            cluster_names[block] = cluster_name;
            block_nodes[block] = block->dot_subgraph(dot, cluster_name);
        }

        for (const FlowGraph::Edge& edge : blocks.edges()) {
            const std::string entry_cluster = cluster_names.at(edge.entry);
            const std::string exit_cluster  = cluster_names.at(edge.exit);
            const std::string entry_node    = block_nodes.at(edge.entry);
            const std::string exit_node     = block_nodes.at(edge.exit);

            dot << entry_node << " -> " << exit_node << " [ltail=\"" << entry_cluster << "\" lhead=\"" << exit_cluster << "\" label=\"" << edge.attr << "\"];\n";
        }

        dot << "}\n";
        return block_nodes.begin()->second;
    }

    void Procedure::build_flow_graph(std::shared_ptr<ir::Scope> scope, const ConstantMap& globals) {
        std::unordered_set<std::shared_ptr<ir::Declaration>> defined_decls;
        for (const auto& [declaration, value] : globals)
            if (declaration->type->category != ir::TypeCategory::FUNCTION)
                defined_decls.insert(declaration);
        for (std::shared_ptr<ir::Declaration> local : scope->locals_list())
            if (!(local->storage & ir::INTERNAL_STORAGE) && local->type->category != ir::TypeCategory::FUNCTION)
                defined_decls.insert(local);

        // Initialize the labeled blocks to have jump destinations
        std::unordered_map<std::string, std::shared_ptr<BasicBlock>> labeled_blocks;
        for (const auto& [name, label] : scope->labels) {
            std::shared_ptr<BasicBlock> block = blocks.emplace_node(BasicBlockType::INNER, unique_id, label);
            labeled_blocks[name] = block;
        }

        // Now we can build the flow graph in one pass
        if (scope->statements.empty() || scope->statements[0].tag != ir::StatementTag::MARKER)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "A function body should start with a label marker", location);

        // Truth table of those :  current              : Within an ongoing block
        //                        !current &&  previous : Right after a conditional jump
        //                        !current && !previous : Right after an unconditional jump
        std::shared_ptr<BasicBlock> previous_block = entry_block;
        std::shared_ptr<BasicBlock> current_block = nullptr;
        for (const auto& [statement_index, statement] : std::ranges::enumerate_view(scope->statements)) {
            // Label = jump destination -> start a new block. FIXME : may benefit from a step to clear orphan labels
            if (statement.tag == ir::StatementTag::MARKER) {
                const std::optional<ir::Label> label = scope->find_label(statement);
                if (!label.has_value())
                    throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Found marker for unknown label {}", statement.output->label()), statement.location);
                current_block = labeled_blocks[label->name];  // The block already exists and has its type and label already set

                // If the previous block may fall through (didn't end in an inconditional jump / return), connect it to the new block
                if (previous_block.get() != nullptr)
                    blocks.add_edge(previous_block, current_block, FlowType::FALLTHROUGH);

                // NOTE : After splitting into local blocks, markers are not relevant anymore since there's at most one label at the beginning of each block
                //        Don't reinsert them
                previous_block = current_block;
                continue;
            } else if (current_block.get() == nullptr && previous_block.get() == nullptr) {
                // We're right after an unconditional jump, and there's no label so nothing can jump here
                // This statement is unreachable, skip it
                continue;
            } else if (current_block.get() == nullptr) {
                // We just exited a block with a conditional jump, so there's no label but we can still fall through from the previous block
                // Create a new block and chain it after the previous block
                current_block = blocks.emplace_node(BasicBlockType::INNER, unique_id);
                blocks.add_edge(previous_block, current_block, FlowType::FALLTHROUGH);
                previous_block = current_block;
            }

            current_block->add_statement(statement, defined_decls);

            if (statement.tag == ir::StatementTag::JUMP || statement.tag == ir::StatementTag::JUMP_IF_TRUE || statement.tag == ir::StatementTag::JUMP_IF_FALSE) {
                // Jump -> exit this block, connect it to the target block
                std::optional<ir::Label> target = scope->find_label(statement.inputs[0].label());
                if (!target.has_value())
                    throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Found jump to unknown label {}", statement.inputs[0].label()), statement.location);
                blocks.add_edge(current_block, labeled_blocks[target->name], FlowType::JUMP);

                // Set the previous block to connect the next block : conditional jump -> allow connections, unconditional jump -> don't
                if (statement.tag == ir::StatementTag::JUMP)  previous_block = nullptr;
                else                                      previous_block = current_block;

                current_block = nullptr;
            } else if (statement.tag == ir::StatementTag::CALL) {
                // Procedure calls may have arbitrary side effects. At least for now, split after calls
                previous_block = current_block;
                current_block = nullptr;
            } else if (statement.tag == ir::StatementTag::RETURN || statement.tag == ir::StatementTag::RETURN_VAL) {
                std::shared_ptr<ir::FunctionType> function_type = std::static_pointer_cast<ir::FunctionType>(declaration->type);
                if (function_type->return_type->category == ir::TypeCategory::VOID && statement.tag == ir::StatementTag::RETURN_VAL)
                    throw Diagnostic(DiagnosticLevel::ERROR, "Return statement with a value in a function with `void` return type", location);
                else if (function_type->return_type->category != ir::TypeCategory::VOID && statement.tag == ir::StatementTag::RETURN)
                    throw Diagnostic(DiagnosticLevel::ERROR, "Return statement without a value in a function with non-void return type", location);

                // Return -> connect to the exit block, don't connect to the next block in the flat code
                blocks.add_edge(current_block, exit_block, FlowType::JUMP);
                current_block  = nullptr;
                previous_block = nullptr;
            }
        }

        std::stringstream dot;
        dot_subgraph(dot);

        // All possible control flow paths should eventually reach the exit block
        // For those who don't, if the function has no return value we can implicitely insert a return statement. Otherwise the function is ill-formed.
        auto insert_implicit_exit = [&](std::shared_ptr<BasicBlock> block) {
            const CodeLocation location = scope->statements.back().location;

            std::shared_ptr<ir::FunctionType> function_type = std::static_pointer_cast<ir::FunctionType>(declaration->type);
            if (function_type->return_type->category == ir::TypeCategory::VOID) {
                block->add_statement(ir::Statement::make_return(location), defined_decls);
                blocks.add_edge(block, exit_block, FlowType::JUMP);
            } else throw Diagnostic(DiagnosticLevel::ERROR, "Some control flow paths reach the end of the function without returning a value", location)
                .add_note(DiagnosticLevel::NOTE, dot.str());
        };

        // Now, ensure that the control frow is correct (all control paths go from the entry block, through inner blocks, to the exit block)
        // First, ensure that all flow control paths are reachable from the entry block, prune those that don't
        // We need to do it first to avoid unreachable blocks from counting as non-returning paths in the next step
        // Unreachable blocks arise naturally from some constructs like if-statements at the end of a function, and they disturb the next steps
        for (std::shared_ptr<BasicBlock> unreachable : blocks.unreachable_from(entry_block))
            blocks.pop_node(unreachable);

        // The last block didn't finish with an unconditional jump / exit -> there should be a return here
        // Treat the last block specially because if it ends with a conditional jump,
        // at this point it is connected to its target label but the fall-through option isn't connected to anything
        if (current_block.get() != nullptr || previous_block.get() != nullptr) {
            std::shared_ptr<BasicBlock> last_block = (current_block.get() == nullptr ? previous_block : current_block);
            if (blocks.contains(last_block))  // Only if it wasn't pruned by the previous step
                insert_implicit_exit(last_block);
        }

        // Next, ensure that all flow control paths lead to the exit block (i.e finish with a `return` statement), to ensure that the control flow is valid
        for (std::shared_ptr<BasicBlock> dead_end : blocks.cannot_reach(exit_block))
            insert_implicit_exit(dead_end);

        // Finish building all blocks
        for (std::shared_ptr<BasicBlock> block : blocks.nodes())
            block->finish();
    }

    std::string Procedure::start_label() const {
        return declaration->name;
    }

    std::string Procedure::end_label() const {
        return std::format(".L{}.BB.__end", declaration->name);
    }


    // Get all local variables used within the procedure
    std::unordered_set<std::shared_ptr<ir::Declaration>> Procedure::locals() const {
        std::unordered_set<std::shared_ptr<ir::Declaration>> declarations;
        for (std::shared_ptr<BasicBlock> block : blocks.nodes())
            declarations.insert_range(block->locals());

        // Parameters must be added explicitely, since unused parameters won't appear in dependency graphs used in BasicBlock::locals
        declarations.insert_range(parameters);
        return declarations;
    }

    // Get all variables that are live through the block
    // Variables that are live upon exit of a previous block, and upon entry of a following block, are live through the `block`
    // That is, the block doesn't use them, but they stay live from a previous block through to a following block
    std::unordered_set<std::shared_ptr<ir::Declaration>> Procedure::live_through(std::shared_ptr<BasicBlock> block) const {
        std::unordered_set<std::shared_ptr<ir::Declaration>> previous_variables;
        previous_variables.insert_range(parameters);
        for (std::shared_ptr<BasicBlock> previous_block : blocks.can_reach(block))
            if (previous_block != block)
                previous_variables.insert_range(previous_block->live_on_exit());

        std::unordered_set<std::shared_ptr<ir::Declaration>> next_variables;
        for (std::shared_ptr<BasicBlock> next_block : blocks.reachable_from(block))
            if (next_block != block)
                next_variables.insert_range(next_block->live_on_entry());

        return unordered_set_intersection(previous_variables, next_variables);
    }

    // Get all variables live upon entry into the block, either within or through that block
    std::unordered_set<std::shared_ptr<ir::Declaration>> Procedure::live_on_entry(std::shared_ptr<BasicBlock> block) const {
        return unordered_set_union(block->live_on_entry(), live_through(block));
    }

    // Get all variables live upon exit from the block, either within or through that block
    std::unordered_set<std::shared_ptr<ir::Declaration>> Procedure::live_on_exit(std::shared_ptr<BasicBlock> block) const {
        return unordered_set_union(block->live_on_exit(), live_through(block));
    }


    // To keep the semantics of conditional jumps, the basic block transitions must always follow fallthrough chains
    // Fallthrough chains make a directed acyclic subgraph of the flow graph
    // Moreover, a block can only fall through to at most one other block, so fallthrough chains are always non-branching paths
    // Return the list of fallthrough chains, in the order they should be generated (entry chain first, exit chain last)
    std::vector<FallthroughChain> Procedure::fallthrough_chains() const {
        // Filter the flow graph to only keep the fallthrough transitions
        FlowGraph fallthrough_graph;
        for (std::shared_ptr<BasicBlock> block : blocks.nodes())
            fallthrough_graph.add_node(block);

        for (const FlowGraph::Edge& transition : blocks.edges())
            if (transition.attr == FlowType::FALLTHROUGH)
                fallthrough_graph.add_edge(transition);

        // Then extract the sequential fallthrough chains
        std::vector<FallthroughChain> chains;
        for (std::shared_ptr<BasicBlock> block : fallthrough_graph.sources()) {
            chains.emplace_back();
            chains.back().push_back(block);

            // Follow the chain
            while (!fallthrough_graph.is_sink(block)) {
                const FlowGraph::EdgeSet fallthrough_transitions = fallthrough_graph.out_edges(block);
                if (fallthrough_transitions.size() != 1)
                    throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Encountered a branching path in the fallthrough subgraph");

                block = fallthrough_transitions.begin()->exit;
                chains.back().push_back(block);
            }
        }

        // Now properly order the chains : entry chain first, exit chain last, for now no particular order for the rest
        auto chain_order = [&](const FallthroughChain& left, const FallthroughChain& right) {
            if      (left.front() == entry_block)   return true;   // Entry chain first
            else if (right.front() == entry_block)  return false;
            else if (left.back() == exit_block)     return false;  // Exit chain last
            else if (right.back() == exit_block)    return true;
            else return true;  // No particular order for the rest
        };

            std::ranges::sort(chains, chain_order);
            return chains;
    }


    // Get whether this is a leaf procedure.
    // A leaf procedure does no call, it's a leaf of the program call tree
    bool Procedure::is_leaf() const {
        for (std::shared_ptr<BasicBlock> block : blocks.nodes())
            if (block->has_calls())
                return false;
        return true;
    }


    // Find which variables are procedure-level or basic block-level temporaries, and really live on exit of blocks
    void Procedure::resolve_intermediates() {
        std::unordered_map<std::shared_ptr<BasicBlock>, std::unordered_set<std::shared_ptr<ir::Declaration>>> live_on_entry;
        std::unordered_map<std::shared_ptr<ir::Declaration>, size_t> nof_uses;

        for (std::shared_ptr<BasicBlock> block : blocks.nodes()) {
            if (block->type != BasicBlockType::INNER)
                continue;
            live_on_entry[block] = block->live_on_entry();

            for (std::shared_ptr<ir::Declaration> variable : block->locals())
                nof_uses[variable] += 1;
        }

        for (const auto& [variable, uses] : nof_uses)
            if (uses == 1)
                variable->storage |= ir::StorageClass::INTERMEDIATE;

        for (std::shared_ptr<BasicBlock> block : blocks.nodes()) {
            std::unordered_set<std::shared_ptr<ir::Declaration>> not_live_on_exit = block->locals();
            for (std::shared_ptr<BasicBlock> reachable : blocks.reachable_from(block))
                if (reachable.get() != block.get())
                    not_live_on_exit = unordered_set_difference(not_live_on_exit, live_on_entry[reachable]);
            block->not_live_on_exit(not_live_on_exit);
        }
    }
}
