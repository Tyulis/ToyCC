#include "flow/block.h"
#include "flow/procedure.h"
#include "flow/unit.h"
#include "ir/type.h"
#include "util/sets.hpp"

namespace toycc::flow {
    // -------- Initial value management
    // Look up an initial value for the variable, set it if it exists.
    static void set_initial_value(std::shared_ptr<DependencyNode> value_node, ConstantMap& initial_constants) {
        auto it = initial_constants.find(value_node->declaration());
        if (it != initial_constants.end()) {
            value_node->value() = it->second;
            initial_constants.erase(it);  // Next instances of this variable won't have the same value, so consume it
        }
    }


    // -------- Input value replacement
    // Replace all read occurences of the `value_node` variable with its known value
    static void replace_value(std::shared_ptr<DependencyNode> statement_node, std::shared_ptr<DependencyNode> value_node, DependencyGraph& graph) {
        ir::Statement& statement = statement_node->statement();
        for (ir::Operand& input : statement.inputs)
            if (input.has_variable_base() && input.declaration() == value_node->declaration())
                input = ir::Operand {value_node->value().value(), input.location};

        if (statement.output.has_value() && statement.output->is_dereference() && statement.output->has_variable_base() && statement.output->declaration() == value_node->declaration())
            statement.output = ir::Operand {value_node->value().value(), statement.output->location};

        // Now that all instances of this value node have been replaced, the statement doesn't depend on the actual variable anymore
        graph.pop_edge(value_node, statement_node);
    }

    // Attempt to replace variable operands with propagated constants
    static void propagate_operands(std::shared_ptr<DependencyNode> statement_node, DependencyGraph& graph) {
        // NOTE : At this point there are no more variable indices, so we don't need to recursively dive into dereference indices
        // ADDRESSOF takes actual variables as inputs, not values, don't replace them
        if (statement_node->statement().tag == ir::StatementTag::ADDRESSOF)
            return;

        for (const DependencyGraph::Edge& input_edge : graph.in_edges(statement_node))
            if (input_edge.attr.type & DependencyType::READ && input_edge.entry->value().has_value())
                replace_value(statement_node, input_edge.entry, graph);
    }


    // -------- Statement evaluation
    template <ir::StatementTag tag>
    bool evaluate_statement(std::shared_ptr<DependencyNode> statement_node, DependencyGraph& graph);

    template<> bool evaluate_statement<ir::StatementTag::COPY> (std::shared_ptr<DependencyNode>, DependencyGraph&) {
        return true;
    }

    // Attempt to evaluate a constant expression statement, return whether the statement must be kept
    static bool evaluate_statement(std::shared_ptr<DependencyNode> statement_node, DependencyGraph& graph) {
        switch (statement_node->statement().tag) {
            case ir::StatementTag::COPY:  return evaluate_statement<ir::StatementTag::COPY>(statement_node, graph);
            default:                      return true;  // Just keep the original statement
        }
    }


    // Attempt to fold operands and evaluate a statement node
    static void fold_statement(std::shared_ptr<DependencyNode> statement_node, DependencyGraph& graph) {
        propagate_operands(statement_node, graph);
        const bool keep_statement = evaluate_statement(statement_node, graph);

        if (!keep_statement)  // The statement has been completely evaluated and doesn't have any effect anymore -> disconnect it
            graph.pop_node(statement_node);
    }

    // -------- BasicBlock
    // Perform constant folding in-place in the block
    // The `initial_constants` are known initial values for some variables
    void BasicBlock::opt_constant_folding(ConstantMap initial_constants) {
        // Evaluate constants in dependency order, i.e following a topological order
        for (std::shared_ptr<DependencyNode> node : dependencies.topological_sort()) {
            if (node->is_value() && dependencies.is_source(node))
                set_initial_value(node, initial_constants);
            else if (node->is_statement())
                fold_statement(node, dependencies);
        }
    }

    // -------- Procedure
    // Build a map of the constants that can be propagated from the given `previous_blocks` to the block they lead to
    ConstantMap cross_block_constants(const FlowGraph::NodeSet& previous_blocks, const ConstantMap& global_constants) {
        std::unordered_set<std::shared_ptr<ir::Declaration>> inconsistent;  // Set of variables that are excluded because they're inconsistent between blocks = unknown in the next block
        ConstantMap consistent = global_constants;

        for (std::shared_ptr<BasicBlock> block : previous_blocks) {
            for (const auto& [variable, value] : block->output_values()) {
                if (!consistent.contains(variable) && !inconsistent.contains(variable)) {  // New constant
                    consistent[variable] = value;
                } else if (consistent.contains(variable) && !inconsistent.contains(variable)) {  // Existing constant, check the consistency
                    if (consistent.at(variable) != value) {  // Inconsistent, exclude it
                        inconsistent.insert(variable);
                        consistent.erase(variable);
                    }
                }
            }
        }

        return consistent;
    }

    // Perform constant folding in-place in each block of the procedure
    // The `global_constants` are known global constants
    void Procedure::opt_constant_folding(const ConstantMap& global_constants) {
        // FIXME : For now, best-effort : go through the graph in breadth-first order, propagate across blocks that are already processed
        //         There's probably better orderings, or a better way to handle cycles
        FlowGraph::NodeSet processed_blocks;

        for (std::shared_ptr<BasicBlock> block : blocks.breadth_first_order()) {
            ConstantMap initial_constants = global_constants;

            // Attempt to propagate constants across blocks : if all previous blocks have been processed, and the constant is the same in all of them, propagate it
            FlowGraph::NodeSet previous_blocks = blocks.previous_nodes(block);
            if (unordered_set_included(previous_blocks, processed_blocks))
                initial_constants = cross_block_constants(previous_blocks, initial_constants);

            block->opt_constant_folding(initial_constants);
        }
    }


    // -------- TranslationUnit
    void TranslationUnit::opt_constant_folding() {
        for (auto& [name, procedure] : procedures)
            procedure.opt_constant_folding(global_block->output_values());
    }
}
