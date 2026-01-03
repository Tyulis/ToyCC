#include <sstream>

#include "diagnostic.h"
#include "ir/flow.h"
#include "ir/type_expressions.h"
#include "util/strings.h"

namespace toycc::ir {
    static std::string local_block_type_repr(LocalBlockType type) {
        switch (type) {
            case LocalBlockType::ENTRY:  return "ENTRY";
            case LocalBlockType::INNER:  return "INNER";
            case LocalBlockType::EXIT:   return "EXIT";
            default: throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unknown local block type");
        }
    }

    // -------- LocalBlock
    LocalBlock::LocalBlock(LocalBlockType type, std::shared_ptr<Label> label) : type(type), label(label) {}

    void LocalBlock::add_statement(std::shared_ptr<Statement> statement, std::unordered_set<std::shared_ptr<Declaration>> available_decls) {
        if (statement->block.get() != nullptr)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Statements in local blocks can't have subblocks", statement->location);

        statements.add_node(statement);

        // Find all inputs and outputs of that statement
        std::unordered_set<std::shared_ptr<Declaration>> inputs;
        std::unordered_set<std::shared_ptr<Declaration>> outputs;

        // FIXME : Function calls may have arbitrary side effects, for now make them full barriers
        if (statement->tag == StatementTag::CALL) {
            inputs = available_decls;
            outputs = available_decls;
        }

        // FIXME : If there's a dereference, don't make any assumption on where that value comes from and make this depend on everything else
        if (statement->output.has_value()) {
            if (statement->output->is_dereference()) {
                outputs.insert_range(available_decls);

                if (!statement->output->has_constant_base())
                    inputs.insert(statement->output->declaration());
            } else if (!statement->output->is_constant()) {
                outputs.insert(statement->output->declaration());
            }
        }

        for (const Operand& input : statement->inputs) {
            if (input.is_dereference())
                inputs.insert_range(available_decls);
            if (!input.has_constant_base())
                inputs.insert(input.declaration());
        }

        // Find where they come from and add edges
        for (std::shared_ptr<Declaration> input : inputs) {
            auto found = last_modification.find(input);

            if (found == last_modification.end()) {
                // Not produced by this block -> link to the entry marker
                input_variables.insert(input);
            } else {
                std::shared_ptr<Statement> origin = found->second;
                DependencyGraph::Edge edge = statements.find_edge(origin, statement).value_or({origin, statement, {}});
                edge.attr.insert(input);
                statements.add_edge(edge);
            }
        }

        for (std::shared_ptr<Declaration> output : outputs) {
            last_modification[output] = statement;
            output_variables.insert(output);
        }
    }

    std::string LocalBlock::ir_code() const {
        std::stringstream code;
        code << local_block_type_repr(type) << " ";
        if (label.get() != nullptr)
            code << label->name << " ";

        code << "[";
        for (const auto [index, declaration] : std::ranges::enumerate_view(input_variables)) {
            code << declaration->name;
            if (static_cast<size_t>(index) != input_variables.size() - 1)
                code << ", ";
        }
        code << "] >>> ";

        if (std::ranges::any_of(statements.nodes(), [](std::shared_ptr<Statement> statement) {return statement->tag != StatementTag::MARKER;})) {
            std::vector<std::shared_ptr<Statement>> order = statements.topological_sort();
            std::unordered_map<std::shared_ptr<Statement>, size_t> statement_indices;
            for (size_t index = 0; index < order.size(); index++)
                statement_indices[order[index]] = index;

            struct StatementLine {
                std::string inputs;
                std::string code;
                std::string outputs;
            };

            std::vector<StatementLine> lines;
            for (size_t index = 0; index < order.size(); index++) {
                const std::shared_ptr<Statement> statement = order[index];
                DependencyGraph::EdgeSet prerequisites = statements.in_edges(statement);
                DependencyGraph::EdgeSet dependents = statements.out_edges(statement);

                std::stringstream inputs, outputs;
                for (const auto [index, edge] : std::ranges::enumerate_view(prerequisites)) {
                    inputs << statement_indices[edge.entry];
                    if (static_cast<size_t>(index) != prerequisites.size() - 1)
                        inputs << ", ";
                }

                for (const auto [index, edge] : std::ranges::enumerate_view(dependents)) {
                    outputs << statement_indices[edge.exit];
                    if (static_cast<size_t>(index) != dependents.size() - 1)
                        outputs << ", ";
                }

                lines.emplace_back(inputs.str(), statement->ir_code(), outputs.str());
            }

            size_t max_inputs_length = 0, max_code_length = 0;
            for (const StatementLine& line : lines) {
                max_inputs_length = std::max(line.inputs.size(), max_inputs_length);
                max_code_length   = std::max(line.code.size(),   max_code_length);
            }

            code << "{\n";
            for (const auto [index, line] : std::ranges::enumerate_view(lines)) {
                code << "    " << index << " : [" << line.inputs << "] ";
                for (size_t position = line.inputs.size(); position < max_inputs_length; position++)
                    code << " ";
                code << ">>> " << line.code;
                for (size_t position = line.code.size(); position < max_code_length; position++)
                    code << " ";
                code << " >>> [" << line.outputs << "];\n";
            }

            code << "} >>> ";
        }

        code << "[";
        for (const auto [index, declaration] : std::ranges::enumerate_view(output_variables)) {
            code << declaration->name;
            if (static_cast<size_t>(index) != output_variables.size() - 1)
                code << ", ";
        }
        code << "]";
        return code.str();
    }

    // -------- Procedure
    Procedure::Procedure(std::shared_ptr<Statement> function) : location(function->location) {
        if (function->tag != StatementTag::FUNCTION)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to initialize a procedure with a statement that's not a function", function->location);

        std::shared_ptr<Scope> scope = function->block;
        declaration = function->output->declaration();
        entry_block = blocks.emplace_node(LocalBlockType::ENTRY);
        exit_block  = blocks.emplace_node(LocalBlockType::EXIT);

        for (std::shared_ptr<Declaration> declaration : scope->locals_list()) {
            if (declaration->storage & StorageClass::PARAMETER)
                parameters.push_back(declaration);
            locals.insert(declaration);
        }

        find_globals(scope);
        build_flow_graph(scope);
    }

    std::string Procedure::ir_code() const {
        std::stringstream code;
        code << "PROCEDURE " << declaration->name << " {\n";

        std::vector<std::shared_ptr<LocalBlock>> block_order;
        std::unordered_map<std::shared_ptr<LocalBlock>, size_t> block_indices;
        auto push_block = [&](std::shared_ptr<LocalBlock> block) {
            if (!block_indices.contains(block)) {
                block_indices[block] = block_order.size();
                block_order.push_back(block);
            }
        };

        blocks.breadth_first_search(push_block);

        for (size_t index = 0; index < block_order.size(); index++) {
            std::shared_ptr<LocalBlock> block = block_order[index];
            code << "    " << index << " : " << indent(block->ir_code(), false, "    ");
            FlowGraph::EdgeSet out_edges = blocks.out_edges(block);

            if (out_edges.size() > 0) {
                code << " -> {";
                size_t exit_index = 0;
                for (FlowGraph::Edge edge : out_edges) {
                    code << block_indices[edge.exit];
                    if (exit_index != out_edges.size() - 1)
                        code << ", ";
                    exit_index += 1;
                }
                code << "}";
            }
            code << ";\n";
        }

        code << "}";
        return code.str();
    }

    void Procedure::find_globals(std::shared_ptr<Scope> scope) {
        for (std::shared_ptr<Statement> statement : scope->statements) {
            // Internal consistency check : at this point, array indices must be constants, this saves us a lot of checks during flow analysis
            for (const Operand& operand : statement->operands())
                for (const Operand& index : operand.indices)
                    if (!index.is_constant())
                        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Upon flow analysis, array indices must be constants", statement->location);

            find_globals(statement);
        }
    }

    void Procedure::find_globals(std::shared_ptr<Statement> statement) {
        for (const Operand& operand : statement->operands())
            if (!operand.has_constant_base() && !locals.contains(operand.declaration()))
                globals.insert(operand.declaration());
    }

    void Procedure::build_flow_graph(std::shared_ptr<Scope> scope) {
        std::unordered_set<std::shared_ptr<Declaration>> used_decls = locals;
        used_decls.insert_range(globals);

        // Initialize the labeled blocks to have jump destinations
        std::unordered_map<std::shared_ptr<Label>, std::shared_ptr<LocalBlock>> labeled_blocks;
        for (std::shared_ptr<Label> label : scope->labels) {
            std::shared_ptr<LocalBlock> block = blocks.emplace_node(LocalBlockType::INNER, label);
            labeled_blocks[label] = block;
        }

        // Now we can build the flow graph in one pass
        if (scope->statements.empty() || scope->statements[0]->tag != StatementTag::MARKER)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "A function body should start with a label marker", location);

        // Truth table of those :  current              : Within an ongoing block
        //                        !current &&  previous : Right after a conditional jump
        //                        !current && !previous : Right after an unconditional jump
        std::shared_ptr<LocalBlock> previous_block = entry_block;
        std::shared_ptr<LocalBlock> current_block = nullptr;
        for (std::shared_ptr<Statement> statement : scope->statements) {
            // Label = jump destination -> start a new block. FIXME : may benefit from a step to clear orphan labels
            if (statement->tag == StatementTag::MARKER) {
                std::shared_ptr<Label> label = scope->find_label(statement);
                current_block = labeled_blocks[label];  // The block already exists and has its type and label already set

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
                current_block = blocks.emplace_node(LocalBlockType::INNER);
                blocks.add_edge(previous_block, current_block, FlowType::FALLTHROUGH);
            }

            current_block->add_statement(statement, used_decls);

            if (statement->tag == StatementTag::JUMP || statement->tag == StatementTag::JUMP_IF_TRUE || statement->tag == StatementTag::JUMP_IF_FALSE) {
                // Jump -> exit this block, connect it to the target block
                std::shared_ptr<Label> target = scope->find_label(*statement->label);
                blocks.add_edge(current_block, labeled_blocks[target], FlowType::JUMP);

                // Set the previous block to connect the next block : conditional jump -> allow connections, unconditional jump -> don't
                if (statement->tag == StatementTag::JUMP)  previous_block = nullptr;
                else                                       previous_block = current_block;

                current_block = nullptr;
            } else if (statement->tag == StatementTag::CALL) {
                // Procedure calls may have arbitrary side effects. At least for now, split after calls
                previous_block = current_block;
                current_block = nullptr;
            } else if (statement->tag == StatementTag::RETURN) {
                // Return -> connect to the exit block, don't connect to the next block in the flat code
                blocks.add_edge(current_block, exit_block, FlowType::JUMP);
                current_block  = nullptr;
                previous_block = nullptr;
            }
        }

        // All possible control flow paths should eventually reach the exit block
        // For those who don't, if the function has no return value we can implicitely insert a return statement. Otherwise the procedure is ill-formed.
        auto insert_implicit_exit = [&](std::shared_ptr<LocalBlock> block) {
            const CodeLocation location = scope->statements.back()->location;

            std::shared_ptr<FunctionType> function_type = std::static_pointer_cast<FunctionType>(declaration->type);
            if (function_type->return_type->category == TypeCategory::VOID) {
                block->add_statement(Statement::make_return(location), used_decls);
                blocks.add_edge(block, exit_block, FlowType::JUMP);
            } else throw Diagnostic(DiagnosticLevel::ERROR, "Some control flow paths reach the end of the function without returning a value", location);
        };

        // Now, ensure that the control frow is correct (all control paths go from the entry block, through inner blocks, to the exit block)
        // First, ensure that all flow control paths are reachable from the entry block, prune those that don't
        // We need to do it first to avoid unreachable blocks from counting as non-returning paths in the next step
        // Unreachable blocks arise naturally from some constructs like if-statements at the end of a function, and they disturb the next steps
        for (std::shared_ptr<LocalBlock> unreachable : blocks.unreachable_from(entry_block))
            blocks.pop_node(unreachable);

        // The last block didn't finish with an unconditional jump / exit -> there should be a return here
        // Treat the last block specially because if it ends with a conditional jump,
        // at this point it is connected to its target label but the fall-through option isn't connected to anything
        if (current_block.get() != nullptr || previous_block.get() != nullptr) {
            std::shared_ptr<LocalBlock> last_block = (current_block.get() == nullptr ? previous_block : current_block);
            if (blocks.contains(last_block))  // Only if it wasn't pruned by the previous step
                insert_implicit_exit(last_block);
        }

        // Next, ensure that all flow control paths lead to the exit block (i.e finish with a `return` statement), to ensure that the control flow is valid
        for (std::shared_ptr<LocalBlock> dead_end : blocks.cannot_reach(exit_block))
            insert_implicit_exit(dead_end);

        // Since we deleted markers, clear markers from the labels
        for (std::shared_ptr<LocalBlock> block : blocks.nodes())
            if (block->label.get() != nullptr)
                block->label->marker = nullptr;
    }

    // -------- TranslationUnit
    std::string TranslationUnit::ir_code() const {
        std::stringstream code;
        for (const auto& [name, declaration] : globals)
            code << declaration->ir_code() << ";\n";

        code << "\n";
        for (const auto& [name, procedure] : procedures)
            code << procedure.ir_code() << "\n\n";

        return code.str();
    }
}
