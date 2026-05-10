#include <sstream>
#include <variant>

#include "config.h"
#include "diagnostic.h"
#include "ir/flow.h"
#include "ir/declaration.h"
#include "ir/type_expressions.h"
#include "util/sets.hpp"
#include "util/graph.hpp"
#include "util/strings.h"

namespace toycc::ir {
    static std::string dependency_type_repr(DependencyType type) {
        switch (type) {
            case DependencyType::READ:          return "READ";
            case DependencyType::WRITE:         return "WRITE";
            case DependencyType::CALL:          return "CALL";
            case DependencyType::DEREFERENCE:   return "DEREFERENCE";
            case DependencyType::LIVE_ON_EXIT:  return "LIVE_ON_EXIT";
        }
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unknown dependency type");
    }

    static std::string operand_group_repr(OperandGroup type) {
        switch (type) {
            case OperandGroup::INDIRECT:  return "INDIRECT";
            case OperandGroup::INPUT:     return "INPUT";
            case OperandGroup::OUTPUT:    return "OUTPUT";
        }
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unknown operand group");
    }

    std::ostream& operator<< (std::ostream& stream, FlowType type) {
        switch (type) {
            case FlowType::FALLTHROUGH:  stream << "FALLTHROUGH";  break;
            case FlowType::JUMP:         stream << "JUMP";         break;
            case FlowType::UNRELATED:    stream << "UNRELATED";    break;
        }
        return stream;
    }

    static std::string dependency_repr(const Dependency& dependency) {
        std::stringstream repr;
        repr << "(";
        size_t type_index = 0;
        for (DependencyType type : dependency.type) {
            if (type_index++ > 0)
                repr << "|";
            repr << dependency_type_repr(type);
        }

        repr << ") " << operand_group_repr(dependency.operand_group);
        if (dependency.operand_group == OperandGroup::INPUT)
            repr << "[" << dependency.operand_index << "]";
        return repr.str();
    }

    std::string dot_graph(const DependencyGraph& graph, std::string cluster_name) {
        std::stringstream dot;
        dot << "digraph " << cluster_name << " {\n";
        size_t node_index = 0;
        std::unordered_map<std::shared_ptr<DependencyNode>, std::string> node_names;
        for (std::shared_ptr<DependencyNode> node : graph.nodes()) {
            const std::string node_name = node_names[node] = std::format("{}_{}", cluster_name, node_index++);
            if (node->is_statement())
                dot << "    " << node_name << " [label=\"" << node->statement().ir_code() << "\" shape=box];\n";
            else if (node->is_value())
                dot << "    " << node_name << " [label=\"" << node->declaration()->name << "\" shape=ellipse];\n";
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown dependency node type");
        }

        for (const DependencyGraph::Edge& edge : graph.edges())
            dot << "    " << node_names[edge.entry] << " -> " << node_names[edge.exit] << " [label=\"" << dependency_repr(edge.attr) << "\"];\n";

        dot << "}";
        return dot.str();
    }

    DependencyMatrix to_dependency_matrix(const DependencyGraph& graph) {
        DependencyMatrix result;
        std::unordered_map<std::shared_ptr<DependencyNode>, size_t> value_indices;
        for (std::shared_ptr<DependencyNode> node : graph.nodes()) {
            if (node->is_statement()) {
                result.statements.push_back(node);
            } else if (node->is_value()) {
                value_indices[node] = result.values.size();
                result.values.push_back(node);
            }
        }

        result.matrix = arma::imat(result.statements.size(), result.values.size(), arma::fill::zeros);
        for (const auto& [row, statement_node] : std::ranges::enumerate_view(result.statements)) {
            for (const DependencyGraph::Edge& input : graph.in_edges(statement_node))
                if (input.attr.operand_group == OperandGroup::INPUT && (input.attr.type & DependencyType::READ))
                    result.matrix(row, value_indices[input.entry]) = 1 + input.attr.operand_index;

            for (const DependencyGraph::Edge& output : graph.out_edges(statement_node))
                if (output.attr.operand_group == OperandGroup::OUTPUT && (output.attr.type & DependencyType::WRITE))
                    result.matrix(row, value_indices[output.exit]) = -(1 + output.attr.operand_index);
        }

        return result;
    }

    std::ostream& operator<< (std::ostream& stream, const DependencyMatrix& graph) {
        size_t statement_width = 0;
        for (std::shared_ptr<DependencyNode> statement : graph.statements)
            statement_width = std::max(statement_width, statement->statement().ir_code().size());
        statement_width += 1;

        stream << justify_right("", statement_width);
        std::vector<size_t> column_widths;
        for (std::shared_ptr<DependencyNode> value : graph.values) {
            const std::string name = value->declaration()->name;
            const size_t column_width = std::max(size_t(2), name.size()) + 1;
            column_widths.push_back(column_width);
            stream << center(name, column_width);
        }
        stream << "\n";

        for (size_t row = 0; row < graph.matrix.n_rows; row++) {
            stream << justify_right(graph.statements[row]->statement().ir_code(), statement_width);
            for (size_t col = 0; col < graph.matrix.n_cols; col++)
                stream << center(std::to_string(graph.matrix(row, col)), column_widths[col]);
            stream << "\n";
        }

        return stream;
    }

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


    // -------- BasicBlock
    BasicBlock::BasicBlock(BasicBlockType type, std::shared_ptr<size_t> unique_id, std::optional<Label> label) : type(type), label(label), unique_id(unique_id) {}

    void BasicBlock::add_statement(const Statement& statement, std::unordered_set<std::shared_ptr<Declaration>> defined_decls) {
        if (statement.block.get() != nullptr)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Statements in local blocks can't have subblocks", statement.location);

        std::shared_ptr<DependencyNode> statement_node = dependencies.emplace_node(statement);
        exit_statement = statement_node;

        // Find all inputs and outputs of that statement
        std::unordered_map<std::shared_ptr<Declaration>, Dependency> inputs;
        std::unordered_map<std::shared_ptr<Declaration>, Dependency> outputs;

        // FIXME : Function calls may have arbitrary side effects, for now make them full barriers
        if (statement.tag == StatementTag::CALL) {
            for (std::shared_ptr<Declaration> decl : defined_decls) {
                inputs [decl].type |= DependencyType::CALL;
                outputs[decl].type |= DependencyType::CALL;
            }
        }

        // FIXME : If there's a dereference, don't make any assumption on where that value comes from and make this depend on everything else
        if (statement.output.has_value()) {
            if (statement.output->is_dereference()) {
                for (std::shared_ptr<Declaration> decl : defined_decls) {
                    inputs [decl].type |= DependencyType::DEREFERENCE;
                    outputs[decl].type |= DependencyType::DEREFERENCE;
                }

                if (statement.output->has_variable_base()) {
                    Dependency& dependency = inputs[statement.output->declaration()];
                    dependency.type |= DependencyType::READ;
                    dependency.operand_group = OperandGroup::OUTPUT;
                    dependency.operand_index = 0;
                }
            } else if (statement.output->is_variable()) {
                Dependency& dependency = outputs[statement.output->declaration()];
                dependency.type |= DependencyType::WRITE;
                dependency.operand_group = OperandGroup::OUTPUT;
                dependency.operand_index = 0;
            }
        }

        for (const auto& [index, input] : std::ranges::enumerate_view(statement.inputs)) {
            if (input.is_dereference())
                for (std::shared_ptr<Declaration> decl : defined_decls)
                    inputs[decl].type |= DependencyType::DEREFERENCE;

            if (input.has_variable_base()) {
                Dependency& dependency = inputs[input.declaration()];
                dependency.type |= DependencyType::READ;
                dependency.operand_group = OperandGroup::INPUT;
                dependency.operand_index = index;
            }
        }

        // Add edges from input variables, adding unknown ones as source nodes
        for (const auto& [input, dependency] : inputs) {
            if (input->storage & ir::StorageClass::GLOBAL)
                used_globals.insert(input);

            auto found = last_modification.find(input);
            std::shared_ptr<DependencyNode> input_node = nullptr;
            if (found == last_modification.end()) {
                input_node = dependencies.emplace_node(input);
                last_modification[input] = input_node;
            } else {
                input_node = found->second;
            }

            dependencies.add_edge(input_node, statement_node, dependency);
        }

        // Add edges to output variables
        for (const auto& [output, dependency] : outputs) {
            if (output->storage & ir::StorageClass::GLOBAL)
                used_globals.insert(output);

            std::shared_ptr<DependencyNode> output_node = dependencies.emplace_node(output);
            dependencies.add_edge(statement_node, output_node, dependency);
            last_modification[output] = output_node;
        }
    }

    void BasicBlock::finish() {
        // Link all variables live on exit to the exit statement
        if (exit_statement.get() != nullptr) {
            for (const auto& [declaration, node] : last_modification) {
                DependencyGraph::Edge exit_edge = dependencies.find_edge(node, exit_statement).value_or(DependencyGraph::Edge {node, exit_statement, {}});
                exit_edge.attr.type |= DependencyType::LIVE_ON_EXIT;

                if (!dependencies.contains(exit_statement, node))
                    dependencies.add_edge(exit_edge);
            }

            for (DependencyGraph::Edge edge : dependencies.connected_edges(exit_statement)) {
                edge.attr.type |= DependencyType::LIVE_ON_EXIT;
                dependencies.add_edge(edge);
            }
        }

        last_modification.clear();
    }

    void BasicBlock::not_live_on_exit(const std::unordered_set<std::shared_ptr<Declaration>>& not_live) {
        for (std::shared_ptr<Declaration> variable : not_live) {
            for (std::shared_ptr<DependencyNode> node : dependencies.find_nodes(variable)) {
                // Eliminate edges to the exit node that are just there to make the variable live on exit
                for (DependencyGraph::Edge edge : dependencies.out_edges(node)) {
                    if (!(edge.attr.type & DependencyType::LIVE_ON_EXIT))
                        continue;

                    edge.attr.type.clear(DependencyType::LIVE_ON_EXIT);
                    if (!edge.attr.type)  dependencies.pop_edge(edge);
                    else                  dependencies.add_edge(edge);  // Replace the flags
                }

                // If that variable was only linked to be live on exit, remove the node
                if (dependencies.is_sink(node))
                    dependencies.pop_node(node);
            }
        }
    }

    static void replace_declaration(Operand& operand, std::shared_ptr<Declaration> initial, std::shared_ptr<Declaration> replacement) {
        if (operand.has_variable_base() && operand.declaration() == initial)
            operand.value = replacement;
    }

    // Make intermediate values of external variables into internal temporaries
    void BasicBlock::split_intermediate_values() {
        for (std::shared_ptr<Declaration> local : locals()) {
            for (std::shared_ptr<DependencyNode> value_node : dependencies.find_nodes(local)) {
                // Values live on entry and exit are not intermediates
                if (dependencies.is_source(value_node) || dependencies.is_sink(value_node))
                    continue;

                Flags<DependencyType> dependency_types;
                for (DependencyGraph::Edge edge : dependencies.connected_edges(value_node))
                    dependency_types |= edge.attr.type;

                // Also keep variables affected by dereferences and calls
                if (dependency_types & (DependencyType::LIVE_ON_EXIT | DependencyType::CALL | DependencyType::DEREFERENCE))
                    continue;

                // At this point, this value node is an intermediate value : replace it with an intermediate declaration
                std::shared_ptr<Declaration> intermediate = declare_intermediate(local->type, local->location);
                value_node->node = intermediate;

                auto replace_operands = [&](std::shared_ptr<DependencyNode> statement_node, const Dependency& dependency) {
                    if (!statement_node->is_statement())
                        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Value node connected to another value node", value_node->location());
                    Statement& statement = statement_node->statement();

                    switch (dependency.operand_group) {
                        case OperandGroup::INDIRECT: return;  // INDIRECT dependency are there to account for side effects of dereferences and calls, they're not actual operands of the statement

                        case OperandGroup::INPUT:
                            for (Operand& input : statement.inputs)
                                replace_declaration(input, local, intermediate);
                        break;

                        case OperandGroup::OUTPUT:
                            if (!statement.output.has_value())
                                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Found an OUTPUT edge to a statement without outputs", value_node->location());
                        replace_declaration(statement.output.value(), local, intermediate);
                    }
                };

                for (DependencyGraph::Edge edge : dependencies.in_edges(value_node))
                    replace_operands(edge.entry, edge.attr);

                for (DependencyGraph::Edge edge : dependencies.out_edges(value_node))
                    replace_operands(edge.exit, edge.attr);
            }
        }
    }

    std::shared_ptr<Declaration> BasicBlock::declare_intermediate(std::shared_ptr<Type> type, CodeLocation location) {
        const std::string name = std::format(".BBI{}", *unique_id);
        *unique_id += 1;
        return std::make_shared<Declaration>(name, type, location, StorageClass::AUTO | StorageClass::TEMPORARY | StorageClass::INTERMEDIATE);
    }

    static std::string local_block_type_repr(BasicBlockType type) {
        switch (type) {
            case BasicBlockType::ENTRY:  return "ENTRY";
            case BasicBlockType::INNER:  return "INNER";
            case BasicBlockType::EXIT:   return "EXIT";
        }
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unknown local block type");
    }

    // Write the graph in dot format to `dot`, return the name of any node in the cluster
    std::string BasicBlock::dot_subgraph(std::stringstream& dot, std::string cluster_name) const {
        dot << "subgraph " << cluster_name << " {\n";
        if (type != BasicBlockType::INNER || dependencies.empty()) {
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
                dot << node_name << " [label=\"" << node->declaration()->ir_code() << "\" shape=ellipse];\n";
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown dependency node type");
        }

        for (const DependencyGraph::Edge& edge : dependencies.edges())
            dot << node_names[edge.entry] << " -> " << node_names[edge.exit] << " [label=\"" << dependency_repr(edge.attr) << "\"];\n";

        dot << "}\n";
        return node_names.begin()->second;
    }

    std::unordered_set<std::shared_ptr<Declaration>> BasicBlock::locals() const {
        std::unordered_set<std::shared_ptr<Declaration>> declarations;
        for (std::shared_ptr<DependencyNode> node : dependencies.nodes())
            if (node->is_value() && !(node->declaration()->storage & StorageClass::GLOBAL))
                declarations.insert(node->declaration());
        return declarations;
    }

    std::unordered_set<std::shared_ptr<Declaration>> BasicBlock::live_on_entry() const {
        std::unordered_set<std::shared_ptr<Declaration>> live;
        for (std::shared_ptr<DependencyNode> source : dependencies.sources())
            if (source->is_value())
                live.insert(source->declaration());
        return live;
    }

    std::unordered_set<std::shared_ptr<Declaration>> BasicBlock::live_on_exit() const {
        std::unordered_set<std::shared_ptr<Declaration>> live;
        for (std::shared_ptr<DependencyNode> sink : dependencies.sinks())
            if (sink->is_value())
                live.insert(sink->declaration());

        for (const DependencyGraph::Edge& edge : dependencies.in_edges(exit_statement))
            if (edge.attr.type & DependencyType::LIVE_ON_EXIT)
                live.insert(edge.entry->declaration());

        return live;
    }


    // -------- Procedure
    Procedure::Procedure(const Statement& function, const GlobalMap& globals, std::shared_ptr<size_t> unique_id)
            : declaration(function.output->declaration()), location(function.location), unique_id(unique_id)
    {
        if (function.tag != StatementTag::FUNCTION)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to initialize a procedure with a statement that's not a function", function.location);

        std::shared_ptr<Scope> scope = function.block;
        entry_block = blocks.emplace_node(BasicBlockType::ENTRY, unique_id);
        exit_block  = blocks.emplace_node(BasicBlockType::EXIT,  unique_id,
                                          Label {.type = LabelType::INTERNAL, .name = std::format(".L{}.BB.__exit", declaration->name), .location = function.location});

        for (std::shared_ptr<Declaration> declaration : scope->locals_list())
            if (declaration->storage & StorageClass::PARAMETER)
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

    void Procedure::build_flow_graph(std::shared_ptr<Scope> scope, const GlobalMap& globals) {
        std::unordered_set<std::shared_ptr<Declaration>> defined_decls;
        for (const auto& [declaration, value] : globals)
            if (declaration->type->category != TypeCategory::FUNCTION)
                defined_decls.insert(declaration);
        for (std::shared_ptr<Declaration> local : scope->locals_list())
            if (!(local->storage & INTERNAL_STORAGE) && local->type->category != TypeCategory::FUNCTION)
                defined_decls.insert(local);

        // Initialize the labeled blocks to have jump destinations
        std::unordered_map<std::string, std::shared_ptr<BasicBlock>> labeled_blocks;
        for (const auto& [name, label] : scope->labels) {
            std::shared_ptr<BasicBlock> block = blocks.emplace_node(BasicBlockType::INNER, unique_id, label);
            labeled_blocks[name] = block;
        }

        // Now we can build the flow graph in one pass
        if (scope->statements.empty() || scope->statements[0].tag != StatementTag::MARKER)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "A function body should start with a label marker", location);

        // Truth table of those :  current              : Within an ongoing block
        //                        !current &&  previous : Right after a conditional jump
        //                        !current && !previous : Right after an unconditional jump
        std::shared_ptr<BasicBlock> previous_block = entry_block;
        std::shared_ptr<BasicBlock> current_block = nullptr;
        for (const auto& [statement_index, statement] : std::ranges::enumerate_view(scope->statements)) {
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
                current_block = blocks.emplace_node(BasicBlockType::INNER, unique_id);
                blocks.add_edge(previous_block, current_block, FlowType::FALLTHROUGH);
                previous_block = current_block;
            }

            current_block->add_statement(statement, defined_decls);

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
            } else if (statement.tag == StatementTag::RETURN || statement.tag == StatementTag::RETURN_VAL) {
                std::shared_ptr<FunctionType> function_type = std::static_pointer_cast<FunctionType>(declaration->type);
                if (function_type->return_type->category == TypeCategory::VOID && statement.tag == StatementTag::RETURN_VAL)
                    throw Diagnostic(DiagnosticLevel::ERROR, "Return statement with a value in a function with `void` return type", location);
                else if (function_type->return_type->category != TypeCategory::VOID && statement.tag == StatementTag::RETURN)
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

            std::shared_ptr<FunctionType> function_type = std::static_pointer_cast<FunctionType>(declaration->type);
            if (function_type->return_type->category == TypeCategory::VOID) {
                block->add_statement(Statement::make_return(location), defined_decls);
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


    std::unordered_set<std::shared_ptr<Declaration>> Procedure::locals() const {
        std::unordered_set<std::shared_ptr<Declaration>> declarations;
        for (std::shared_ptr<BasicBlock> block : blocks.nodes())
            declarations.insert_range(block->locals());

        // Parameters must be added explicitely, since unused parameters won't appear in dependency graphs used in BasicBlock::locals
        declarations.insert_range(parameters);
        return declarations;
    }

    // Find which variables are procedure-level or basic block-level temporaries, and really live on exit of blocks
    void Procedure::resolve_intermediates() {
        std::unordered_map<std::shared_ptr<BasicBlock>, std::unordered_set<std::shared_ptr<Declaration>>> live_on_entry;
        std::unordered_map<std::shared_ptr<Declaration>, size_t> nof_uses;

        for (std::shared_ptr<BasicBlock> block : blocks.nodes()) {
            if (block->type != BasicBlockType::INNER)
                continue;
            live_on_entry[block] = block->live_on_entry();

            for (std::shared_ptr<Declaration> variable : block->locals())
                nof_uses[variable] += 1;
        }

        for (const auto& [variable, uses] : nof_uses)
            if (uses == 1)
                variable->storage |= StorageClass::INTERMEDIATE;

        for (std::shared_ptr<BasicBlock> block : blocks.nodes()) {
            std::unordered_set<std::shared_ptr<Declaration>> not_live_on_exit = block->locals();
            for (std::shared_ptr<BasicBlock> reachable : blocks.reachable_from(block))
                if (reachable.get() != block.get())
                    not_live_on_exit = unordered_set_difference(not_live_on_exit, live_on_entry[reachable]);
            block->not_live_on_exit(not_live_on_exit);

            if (toycc::config::optimization::split_intermediates)
                block->split_intermediate_values();
        }
    }

    // -------- TranslationUnit
    TranslationUnit::TranslationUnit(std::shared_ptr<Scope> global_scope, std::string working_directory, std::string filename)
        : working_directory(working_directory), filename(filename), unique_id(std::make_shared<size_t>(0))
    {
        // After descoping, only procedures and static declarations remain
        for (std::shared_ptr<Declaration> declaration : global_scope->locals_list()) {
            declaration->storage = StorageClass::GLOBAL;
            globals[declaration] = {};
        }

        for (const Statement& statement : global_scope->statements) {
            switch (statement.tag) {
                case StatementTag::FUNCTION: {
                    std::shared_ptr<Declaration> function = statement.output->declaration();
                    procedures[function->name] = Procedure {statement, globals, unique_id};
                    break;
                }

                case StatementTag::COPY: {
                    if (!statement.inputs[0].is_constant())
                        throw Diagnostic(DiagnosticLevel::ERROR, "Global initializers must be constants", statement.location);
                    if (!statement.output->is_variable())
                        throw Diagnostic(DiagnosticLevel::ERROR, "Global initializers must be assigned to global variables", statement.location);

                    auto found = globals.find(statement.output->declaration());
                    if (found == globals.end())
                        throw Diagnostic(DiagnosticLevel::ERROR, "Global initializers must be assigned to global variables", statement.location);

                    found->second = statement.inputs[0].constant();
                    break;
                }

                default:  throw Diagnostic(DiagnosticLevel::ERROR, std::format("{} can't be a global statement", statement.ir_code()), statement.location);
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
