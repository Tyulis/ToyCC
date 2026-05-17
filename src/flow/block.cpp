#include "diagnostic.h"
#include "flow/block.h"
#include "util/strings.h"

namespace toycc::flow {
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
        return std::holds_alternative<ir::Statement>(node);
    }

    bool DependencyNode::is_value() const {
        return std::holds_alternative<std::shared_ptr<ir::Declaration>>(node);
    }

    CodeLocation DependencyNode::location() const {
        if      (is_statement())  return statement().location;
        else if (is_value())      return declaration()->location;
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown dependency node type");
    }

    ir::Statement& DependencyNode::statement() {
        return std::get<ir::Statement>(node);
    }

    const ir::Statement& DependencyNode::statement() const {
        return std::get<ir::Statement>(node);
    }

    std::shared_ptr<ir::Declaration> DependencyNode::declaration() const {
        return std::get<std::shared_ptr<ir::Declaration>>(node);
    }

    bool DependencyNode::operator== (const DependencyNode& rhs) const {
        if (is_value() && rhs.is_value())
            return declaration() == rhs.declaration();
        return false;
    }

    bool DependencyNode::operator== (std::shared_ptr<ir::Declaration> variable) const {
        return is_value() && declaration() == variable;
    }


    // -------- BasicBlock
    BasicBlock::BasicBlock(BasicBlockType type, std::shared_ptr<size_t> unique_id, std::optional<ir::Label> label) : type(type), label(label), unique_id(unique_id) {}

    void BasicBlock::add_statement(const ir::Statement& statement, std::unordered_set<std::shared_ptr<ir::Declaration>> defined_decls) {
        if (statement.block.get() != nullptr)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Statements in local blocks can't have subblocks", statement.location);

        std::shared_ptr<DependencyNode> statement_node = dependencies.emplace_node(statement);
        exit_statement = statement_node;

        // Find all inputs and outputs of that statement
        std::unordered_map<std::shared_ptr<ir::Declaration>, Dependency> inputs;
        std::unordered_map<std::shared_ptr<ir::Declaration>, Dependency> outputs;

        // FIXME : Function calls may have arbitrary side effects, for now make them full barriers
        if (statement.tag == ir::StatementTag::CALL) {
            for (std::shared_ptr<ir::Declaration> decl : defined_decls) {
                inputs [decl].type |= DependencyType::CALL;
                outputs[decl].type |= DependencyType::CALL;
            }
        }

        // FIXME : If there's a dereference, don't make any assumption on where that value comes from and make this depend on everything else
        if (statement.output.has_value()) {
            if (statement.output->is_dereference()) {
                for (std::shared_ptr<ir::Declaration> decl : defined_decls) {
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
                for (std::shared_ptr<ir::Declaration> decl : defined_decls)
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

    void BasicBlock::not_live_on_exit(const std::unordered_set<std::shared_ptr<ir::Declaration>>& not_live) {
        for (std::shared_ptr<ir::Declaration> variable : not_live) {
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

    static void replace_declaration(ir::Operand& operand, std::shared_ptr<ir::Declaration> initial, std::shared_ptr<ir::Declaration> replacement) {
        if (operand.has_variable_base() && operand.declaration() == initial)
            operand.value = replacement;
    }

    // Make intermediate values of external variables into internal temporaries
    void BasicBlock::split_intermediate_values() {
        for (std::shared_ptr<ir::Declaration> local : locals()) {
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
                std::shared_ptr<ir::Declaration> intermediate = declare_intermediate(local->type, local->location);
                value_node->node = intermediate;

                auto replace_operands = [&](std::shared_ptr<DependencyNode> statement_node, const Dependency& dependency) {
                    if (!statement_node->is_statement())
                        throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Value node connected to another value node", value_node->location());
                    ir::Statement& statement = statement_node->statement();

                    switch (dependency.operand_group) {
                        case OperandGroup::INDIRECT: return;  // INDIRECT dependency are there to account for side effects of dereferences and calls, they're not actual operands of the statement

                        case OperandGroup::INPUT:
                            for (ir::Operand& input : statement.inputs)
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

    std::shared_ptr<ir::Declaration> BasicBlock::declare_intermediate(std::shared_ptr<ir::Type> type, CodeLocation location) {
        const std::string name = std::format(".BBI{}", *unique_id);
        *unique_id += 1;
        return std::make_shared<ir::Declaration>(name, type, location, ir::StorageClass::AUTO | ir::StorageClass::TEMPORARY | ir::StorageClass::INTERMEDIATE);
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

    std::unordered_set<std::shared_ptr<ir::Declaration>> BasicBlock::locals() const {
        std::unordered_set<std::shared_ptr<ir::Declaration>> declarations;
        for (std::shared_ptr<DependencyNode> node : dependencies.nodes())
            if (node->is_value() && !(node->declaration()->storage & ir::StorageClass::GLOBAL))
                declarations.insert(node->declaration());
        return declarations;
    }

    // Get which variables *used by the block* are live upon entry
    std::unordered_set<std::shared_ptr<ir::Declaration>> BasicBlock::live_on_entry() const {
        std::unordered_set<std::shared_ptr<ir::Declaration>> live;
        for (std::shared_ptr<DependencyNode> source : dependencies.sources())
            if (source->is_value())
                live.insert(source->declaration());
        return live;
    }

    // Get which variables *used by the block* are live upon exit
    std::unordered_set<std::shared_ptr<ir::Declaration>> BasicBlock::live_on_exit() const {
        std::unordered_set<std::shared_ptr<ir::Declaration>> live;
        for (std::shared_ptr<DependencyNode> sink : dependencies.sinks())
            if (sink->is_value())
                live.insert(sink->declaration());

        for (const DependencyGraph::Edge& edge : dependencies.in_edges(exit_statement))
            if (edge.attr.type & DependencyType::LIVE_ON_EXIT)
                live.insert(edge.entry->declaration());

        return live;
    }


    // Get whether any statement in the block is a call
    bool BasicBlock::has_calls() const {
        for (std::shared_ptr<DependencyNode> node : dependencies.nodes())
            if (node->is_statement())
                if (node->statement().tag == ir::StatementTag::CALL)
                    return true;
        return false;
    }
}
