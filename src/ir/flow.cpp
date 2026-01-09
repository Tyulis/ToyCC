#include <sstream>
#include <variant>

#include "diagnostic.h"
#include "ir/flow.h"
#include "ir/declaration.h"
#include "ir/type_expressions.h"

namespace toycc::ir {
    // -------- DependencyNode
    bool DependencyNode::is_statement() const {
        return std::holds_alternative<Statement>(node);
    }

    bool DependencyNode::is_value() const {
        return std::holds_alternative<std::shared_ptr<Declaration>>(node);
    }

    CodeLocation DependencyNode::location() const {
        if      (is_statement())  return statement().location;
        else if (is_value())      return declaration()->location;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown dependency node type");
    }

    Statement& DependencyNode::statement() {
        return std::get<Statement>(node);
    }

    const Statement& DependencyNode::statement() const {
        return std::get<Statement>(node);
    }

    std::shared_ptr<Declaration> DependencyNode::declaration() const {
        return std::get<std::shared_ptr<Declaration>>(node);
    }

    bool DependencyNode::operator== (const DependencyNode& rhs) const {
        if (is_value() && rhs.is_value())
            return declaration() == rhs.declaration();
        return false;
    }

    bool DependencyNode::operator== (std::shared_ptr<Declaration> variable) const {
        return is_value() && declaration() == variable;
    }


    // -------- LocalBlock
    LocalBlock::LocalBlock(LocalBlockType type, std::optional<Label> label) : type(type), label(label) {}

    void LocalBlock::add_statement(const Statement& statement, std::unordered_set<std::shared_ptr<Declaration>> available_decls) {
        if (statement.block.get() != nullptr)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Statements in local blocks can't have subblocks", statement.location);

        std::shared_ptr<DependencyNode> statement_node = dependencies.emplace_node(statement);

        // Find all inputs and outputs of that statement
        std::unordered_set<std::shared_ptr<Declaration>> inputs;
        std::unordered_set<std::shared_ptr<Declaration>> outputs;

        // FIXME : Function calls may have arbitrary side effects, for now make them full barriers
        if (statement.tag == StatementTag::CALL) {
            inputs = available_decls;
            outputs = available_decls;
        }

        // FIXME : If there's a dereference, don't make any assumption on where that value comes from and make this depend on everything else
        if (statement.output.has_value()) {
            if (statement.output->is_dereference()) {
                outputs.insert_range(available_decls);

                if (statement.output->has_variable_base())
                    inputs.insert(statement.output->declaration());
            } else if (statement.output->is_variable()) {
                outputs.insert(statement.output->declaration());
            }
        }

        for (const Operand& input : statement.inputs) {
            if (input.is_dereference())
                inputs.insert_range(available_decls);
            if (input.has_variable_base())
                inputs.insert(input.declaration());
        }

        // Add edges from input variables, adding unknown ones as source nodes
        for (std::shared_ptr<Declaration> input : inputs) {
            auto found = last_modification.find(input);
            std::shared_ptr<DependencyNode> input_node = nullptr;
            if (found == last_modification.end()) {
                input_node = dependencies.emplace_node(input);
                last_modification[input] = input_node;
            } else {
                input_node = found->second;
            }

            dependencies.add_edge(input_node, statement_node);
        }

        // Add edges to output variables
        for (std::shared_ptr<Declaration> output : outputs) {
            std::shared_ptr<DependencyNode> output_node = dependencies.emplace_node(output);
            dependencies.add_edge(statement_node, output_node);
            last_modification[output] = output_node;
        }
    }

    static std::string local_block_type_repr(LocalBlockType type) {
        switch (type) {
            case LocalBlockType::ENTRY:  return "ENTRY";
            case LocalBlockType::INNER:  return "INNER";
            case LocalBlockType::EXIT:   return "EXIT";
            default: throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unknown local block type");
        }
    }

    // Write the graph in dot format to `dot`, return the name of any node in the cluster
    std::string LocalBlock::dot_subgraph(std::stringstream& dot, std::string cluster_name) const {
        dot << "subgraph " << cluster_name << " {\n";
        if (type != LocalBlockType::INNER) {
            const std::string type_repr = local_block_type_repr(type);
            dot << "label = \"" << type_repr << "\";\n";

            std::string node = std::format("{}_{}", cluster_name, type_repr);
            dot << node << "[shape=point style=invis width=0 height=0 margin=0 periphery=0];\n";
            dot << "};\n";
            return node;
        }

        if (label.has_value())
            dot << "label = \"" << label->name << "\";\n";

        size_t node_index = 0;
        std::unordered_map<std::shared_ptr<DependencyNode>, std::string> node_names;
        for (std::shared_ptr<DependencyNode> node : dependencies.nodes()) {
            const std::string node_name = node_names[node] = std::format("{}_{}", cluster_name, node_index++);
            if (node->is_statement())
                dot << node_name << " [label=\"" << node->statement().ir_code() << "\" shape=box];\n";
            else if (node->is_value())
                dot << node_name << " [label=\"" << node->declaration()->name << "\" shape=ellipse];\n";
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown dependency node type");
        }

        for (const DependencyGraph::Edge& edge : dependencies.edges())
            dot << node_names[edge.entry] << " -> " << node_names[edge.exit] << ";\n";

        dot << "}\n";
        return node_names.begin()->second;
    }

    std::unordered_set<std::shared_ptr<Declaration>> LocalBlock::locals() const {
        std::unordered_set<std::shared_ptr<Declaration>> declarations;
        for (std::shared_ptr<DependencyNode> node : dependencies.nodes())
            if (node->is_value() && !(node->declaration()->storage & StorageClass::GLOBAL))
                declarations.insert(node->declaration());
        return declarations;
    }

    // -------- Procedure
    Procedure::Procedure(const Statement& function, const std::unordered_set<std::shared_ptr<Declaration>>& globals)
            : declaration(function.output->declaration()), location(function.location)
    {
        if (function.tag != StatementTag::FUNCTION)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to initialize a procedure with a statement that's not a function", function.location);

        std::shared_ptr<Scope> scope = function.block;
        entry_block = blocks.emplace_node(LocalBlockType::ENTRY);
        exit_block  = blocks.emplace_node(LocalBlockType::EXIT);

        for (std::shared_ptr<Declaration> declaration : scope->locals_list())
            if (declaration->storage & StorageClass::PARAMETER)
                parameters.push_back(declaration);

        build_flow_graph(scope, globals);
    }

    // Write the graph in dot format to `dot`, return the name of any node in the cluster
    std::string Procedure::dot_subgraph(std::stringstream& dot) const {
        const std::string procedure_cluster = std::format("cluster_{}", declaration->name);
        dot << "subgraph " << procedure_cluster << " {\n";
        dot << "label = \"" << declaration->name << "\";\n";

        size_t block_index = 0;
        std::unordered_map<std::shared_ptr<LocalBlock>, std::string> cluster_names;
        std::unordered_map<std::shared_ptr<LocalBlock>, std::string> block_nodes;
        for (std::shared_ptr<LocalBlock> block : blocks.nodes()) {
            const std::string cluster_name = std::format("{}_{}", procedure_cluster, block_index++);
            cluster_names[block] = cluster_name;
            block_nodes[block] = block->dot_subgraph(dot, cluster_name);
        }

        for (const FlowGraph::Edge& edge : blocks.edges()) {
            const std::string entry_cluster = cluster_names.at(edge.entry);
            const std::string exit_cluster = cluster_names.at(edge.exit);
            const std::string entry_node = block_nodes.at(edge.entry);
            const std::string exit_node = block_nodes.at(edge.exit);

            dot << entry_node << " -> " << exit_node << " [ltail=\"" << entry_cluster << "\" lhead=\"" << exit_cluster << "\"];\n";
        }

        dot << "}\n";
        return block_nodes.begin()->second;
    }

    void Procedure::build_flow_graph(std::shared_ptr<Scope> scope, const std::unordered_set<std::shared_ptr<Declaration>>& globals) {
        std::unordered_set<std::shared_ptr<Declaration>> available_decls(globals);
        for (std::shared_ptr<Declaration> local : scope->locals_list())
            available_decls.insert(local);

        // Initialize the labeled blocks to have jump destinations
        std::unordered_map<std::string, std::shared_ptr<LocalBlock>> labeled_blocks;
        for (const auto& [name, label] : scope->labels) {
            std::shared_ptr<LocalBlock> block = blocks.emplace_node(LocalBlockType::INNER, label);
            labeled_blocks[name] = block;
        }

        // Now we can build the flow graph in one pass
        if (scope->statements.empty() || scope->statements[0].tag != StatementTag::MARKER)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "A function body should start with a label marker", location);

        // Truth table of those :  current              : Within an ongoing block
        //                        !current &&  previous : Right after a conditional jump
        //                        !current && !previous : Right after an unconditional jump
        std::shared_ptr<LocalBlock> previous_block = entry_block;
        std::shared_ptr<LocalBlock> current_block = nullptr;
        for (const Statement& statement : scope->statements) {
            // Label = jump destination -> start a new block. FIXME : may benefit from a step to clear orphan labels
            if (statement.tag == StatementTag::MARKER) {
                const std::optional<Label> label = scope->find_label(statement);
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
                current_block = blocks.emplace_node(LocalBlockType::INNER);
                blocks.add_edge(previous_block, current_block, FlowType::FALLTHROUGH);
            }

            current_block->add_statement(statement, available_decls);

            if (statement.tag == StatementTag::JUMP || statement.tag == StatementTag::JUMP_IF_TRUE || statement.tag == StatementTag::JUMP_IF_FALSE) {
                // Jump -> exit this block, connect it to the target block
                std::optional<Label> target = scope->find_label(statement.inputs[0].label());
                if (!target.has_value())
                    throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Found jump to unknown label {}", statement.inputs[0].label()), statement.location);
                blocks.add_edge(current_block, labeled_blocks[target->name], FlowType::JUMP);

                // Set the previous block to connect the next block : conditional jump -> allow connections, unconditional jump -> don't
                if (statement.tag == StatementTag::JUMP)  previous_block = nullptr;
                else                                      previous_block = current_block;

                current_block = nullptr;
            } else if (statement.tag == StatementTag::CALL) {
                // Procedure calls may have arbitrary side effects. At least for now, split after calls
                previous_block = current_block;
                current_block = nullptr;
            } else if (statement.tag == StatementTag::RETURN) {
                // Return -> connect to the exit block, don't connect to the next block in the flat code
                blocks.add_edge(current_block, exit_block, FlowType::JUMP);
                current_block  = nullptr;
                previous_block = nullptr;
            }
        }

        // All possible control flow paths should eventually reach the exit block
        // For those who don't, if the function has no return value we can implicitely insert a return statement. Otherwise the procedure is ill-formed.
        auto insert_implicit_exit = [&](std::shared_ptr<LocalBlock> block) {
            const CodeLocation location = scope->statements.back().location;

            std::shared_ptr<FunctionType> function_type = std::static_pointer_cast<FunctionType>(declaration->type);
            if (function_type->return_type->category == TypeCategory::VOID) {
                block->add_statement(Statement::make_return(location), available_decls);
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
    }

    std::unordered_set<std::shared_ptr<Declaration>> Procedure::locals() const {
        std::unordered_set<std::shared_ptr<Declaration>> declarations;
        for (std::shared_ptr<LocalBlock> block : blocks.nodes())
            declarations.insert_range(block->locals());
        return declarations;
    }

    // -------- TranslationUnit
    TranslationUnit::TranslationUnit(std::shared_ptr<Scope> global_scope) {
        // After descoping, only procedures and static declarations remain
        for (std::shared_ptr<Declaration> declaration : global_scope->locals_list()) {
            declaration->storage = StorageClass::GLOBAL;
            globals.insert(declaration);
        }

        for (const Statement& statement : global_scope->statements) {
            if (statement.tag == StatementTag::FUNCTION) {
                std::shared_ptr<Declaration> function = statement.output->declaration();
                procedures[function->name] = Procedure {statement, globals};
            }
        }
    }

    std::string TranslationUnit::dot_graph() const {
        std::stringstream dot;
        dot << "digraph {\n";
        dot << "compound = true;\n";
        for (const auto& [name, procedure] : procedures)
            procedure.dot_subgraph(dot);
        dot << "}\n";
        return dot.str();
    }
}
