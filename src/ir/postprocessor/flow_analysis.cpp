#include "diagnostic.h"
#include "ir/postprocessor.h"
#include "ir/type_expressions.h"

namespace toycc::ir {
    TranslationUnit PostProcessor::analyse_flow(std::shared_ptr<Scope> global_scope) {
        TranslationUnit unit;

        // After descoping, only procedures and static declarations remain
        for (std::shared_ptr<Declaration> declaration : global_scope->locals_list())
            unit.globals[declaration->name] = declaration;

        for (std::shared_ptr<Statement> statement : global_scope->statements) {
            if (statement->tag == StatementTag::FUNCTION) {
                std::shared_ptr<Declaration> function = statement->output->base.declaration();
                unit.procedures[function->name] = analyse_procedure_flow(statement);
            }
        }

        return unit;
    }

    Procedure PostProcessor::analyse_procedure_flow(std::shared_ptr<Statement> function) {
        std::shared_ptr<Scope> scope = function->block;
        Procedure procedure(function->output->base.declaration(), scope->labels);

        // Initialize the labeled blocks to have jump destinations
        std::unordered_map<std::shared_ptr<Label>, std::shared_ptr<LocalBlock>> labeled_blocks;
        for (std::shared_ptr<Label> label : procedure.labels) {
            std::shared_ptr<LocalBlock> block = procedure.blocks.emplace_node(LocalBlockType::INNER, label);
            labeled_blocks[label] = block;
        }

        // Now we can build the flow graph in one pass
        if (function->block->statements.empty() || function->block->statements[0]->tag != StatementTag::MARKER)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "A function body should start with a label marker", function->location);

        // Truth table of those :  current              : Within an ongoing block
        //                        !current &&  previous : Right after a conditional jump
        //                        !current && !previous : Right after an unconditional jump
        std::shared_ptr<LocalBlock> previous_block = procedure.entry_block;
        std::shared_ptr<LocalBlock> current_block = nullptr;
        for (std::shared_ptr<Statement> statement : function->block->statements) {
            // Label = jump destination -> start a new block. FIXME : may benefit from a step to clear orphan labels
            if (statement->tag == StatementTag::MARKER) {
                std::shared_ptr<Label> label = procedure.find_label(statement);
                current_block = labeled_blocks[label];  // The block already exists and has its type and label already set

                // If the previous block may fall through (didn't end in an inconditional jump / return), connect it to the new block
                if (previous_block.get() != nullptr)
                    procedure.blocks.add_edge(previous_block, current_block);
                previous_block = current_block;

                current_block->statements.push_back(statement);
                continue;
            } else if (current_block.get() == nullptr && previous_block.get() == nullptr) {
                // We're right after an unconditional jump, and there's no label so nothing can jump here
                // This statement is unreachable, skip it
                continue;
            } else if (current_block.get() == nullptr) {
                // We just exited a block with a conditional jump, so there's no label but we can still fall through from the previous block
                // Create a new block and chain it after the previous block
                current_block = procedure.blocks.emplace_node(LocalBlockType::INNER);
                procedure.blocks.add_edge(previous_block, current_block);
            }

            current_block->statements.push_back(statement);

            if (statement->tag == StatementTag::JUMP || statement->tag == StatementTag::JUMP_IF_TRUE || statement->tag == StatementTag::JUMP_IF_FALSE) {
                // Jump -> exit this block, connect it to the target block
                std::shared_ptr<Label> target = procedure.find_label(*statement->label);
                procedure.blocks.add_edge(current_block, labeled_blocks[target]);

                // Set the previous block to connect the next block : conditional jump -> allow connections, unconditional jump -> don't
                if (statement->tag == StatementTag::JUMP)  previous_block = nullptr;
                else                                       previous_block = current_block;

                current_block = nullptr;
            } else if (statement->tag == StatementTag::RETURN) {
                // Return -> connect to the exit block, don't connect to the next block in the flat code
                procedure.blocks.add_edge(current_block, procedure.exit_block);
                current_block  = nullptr;
                previous_block = nullptr;
            }
        }

        // All possible control flow paths should eventually reach the exit block
        // For those who don't, if the function has no return value we can implicitely insert a return statement. Otherwise the procedure is ill-formed.
        auto insert_implicit_exit = [&](std::shared_ptr<LocalBlock> block) {
            const CodeLocation location = block->statements.back()->location;  // The algorithm above can only produce non-empty blocks

            std::shared_ptr<FunctionType> function_type = std::static_pointer_cast<FunctionType>(procedure.declaration->type);
            if (function_type->return_type->category == TypeCategory::VOID) {
                block->statements.push_back(Statement::make_return(location));
                procedure.blocks.add_edge(block, procedure.exit_block);
            } else throw Diagnostic(DiagnosticLevel::ERROR, "Some control flow paths reach the end of the function without returning a value", location);
        };

        // Now, ensure that the control frow is correct (all control paths go from the entry block, through inner blocks, to the exit block)
        // First, ensure that all flow control paths are reachable from the entry block, prune those that don't
        // We need to do it first to avoid unreachable blocks from counting as non-returning paths in the next step
        // Unreachable blocks arise naturally from some constructs like if-statements at the end of a function, and they disturb the next steps
        auto unreachable_blocks = procedure.blocks.unreachable_from(procedure.entry_block);
        for (std::shared_ptr<LocalBlock> unreachable : procedure.blocks.unreachable_from(procedure.entry_block))
            procedure.blocks.pop_node(unreachable);

        // The last block didn't finish with an unconditional jump / exit -> there should be a return here
        // Treat the last block specially because if it ends with a conditional jump,
        // at this point it is connected to its target label but the fall-through option isn't connected to anything
        if (current_block.get() != nullptr || previous_block.get() != nullptr) {
            std::shared_ptr<LocalBlock> last_block = (current_block.get() == nullptr ? previous_block : current_block);
            if (procedure.blocks.contains(last_block))  // Only if it wasn't pruned by the previous step
                insert_implicit_exit(last_block);
        }

        // Next, ensure that all flow control paths lead to the exit block (i.e finish with a `return` statement), to ensure that the control flow is valid
        for (std::shared_ptr<LocalBlock> dead_end : procedure.blocks.cannot_reach(procedure.exit_block))
            insert_implicit_exit(dead_end);

        return procedure;
    }
}
