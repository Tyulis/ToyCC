#include "diagnostic.h"
#include "flow/block.h"

namespace toycc::flow {
    // -------- BasicBlock
    BasicBlock::BasicBlock(BasicBlockType type, std::shared_ptr<size_t> unique_id, std::optional<ir::Label> label) : type(type), label(label), unique_id(unique_id) {}

    void BasicBlock::add_statement(const ir::Statement& statement, const std::unordered_set<std::shared_ptr<ir::Declaration>>& defined_decls) {
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
            dot << node_names[edge.entry] << " -> " << node_names[edge.exit] << " [label=\"" << edge.attr << "\"];\n";

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


    // Get a map of known constant values at the end of a block after constant propagation
    ConstantMap BasicBlock::output_constants() const {
        ConstantMap constants;
        for (std::shared_ptr<DependencyNode> node : dependencies.sinks())
            if (node->is_value() && node->value().has_value())
                constants[node->declaration()] = node->value().value();

        for (DependencyGraph::Edge edge : dependencies.edges())
            if (edge.attr.type & DependencyType::LIVE_ON_EXIT && edge.entry->is_value() && edge.entry->value().has_value())
                constants[edge.entry->declaration()] = edge.entry->value().value();

        return constants;
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
