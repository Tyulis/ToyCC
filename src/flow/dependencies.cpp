#include "diagnostic.h"
#include "flow/dependencies.h"
#include "util/strings.h"

namespace toycc::flow {
    std::ostream& operator<< (std::ostream& stream, DependencyType value) {
        switch (value) {
            case DependencyType::READ:          return (stream << "READ");
            case DependencyType::WRITE:         return (stream << "WRITE");
            case DependencyType::CALL:          return (stream << "CALL");
            case DependencyType::DEREFERENCE:   return (stream << "DEREFERENCE");
            case DependencyType::LIVE_ON_EXIT:  return (stream << "LIVE_ON_EXIT");
        }

        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unknown dependency type");
    }

    std::ostream& operator<< (std::ostream& stream, OperandGroup value) {
        switch (value) {
            case OperandGroup::INDIRECT:  return (stream << "INDIRECT");
            case OperandGroup::INPUT:     return (stream << "INPUT");
            case OperandGroup::OUTPUT:    return (stream << "OUTPUT");
        }
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unknown operand group");
    }

    std::ostream& operator<< (std::ostream& stream, const Dependency& dependency) {
        stream << "(";
        size_t type_index = 0;
        for (DependencyType type : dependency.type) {
            if (type_index++ > 0)
                stream << "|";
            stream << type;
        }

        stream << ") " << dependency.operand_group;
        if (dependency.operand_group == OperandGroup::INPUT)
            stream << "[" << dependency.operand_index << "]";
        return stream;
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
            dot << "    " << node_names[edge.entry] << " -> " << node_names[edge.exit] << " [label=\"" << edge.attr << "\"];\n";

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
    DependencyNode::DependencyNode(const ir::Statement& statement) : node(statement) {}
    DependencyNode::DependencyNode(std::shared_ptr<ir::Declaration> variable) : node(ValueNode {variable, {}}) {}
    DependencyNode::DependencyNode(std::shared_ptr<ir::Declaration> variable, ir::Constant value) : node(ValueNode {variable, value}) {}

    bool DependencyNode::is_statement() const {
        return std::holds_alternative<ir::Statement>(node);
    }

    bool DependencyNode::is_value() const {
        return std::holds_alternative<ValueNode>(node);
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
        return std::get<ValueNode>(node).variable;
    }

    const std::optional<ir::Constant>& DependencyNode::value() const {
        return std::get<ValueNode>(node).value;
    }

    std::optional<ir::Constant>& DependencyNode::value() {
        return std::get<ValueNode>(node).value;
    }

    bool DependencyNode::operator== (const DependencyNode& rhs) const {
        if (is_value() && rhs.is_value())
            return declaration() == rhs.declaration();
        return false;
    }

    bool DependencyNode::operator== (std::shared_ptr<ir::Declaration> variable) const {
        return is_value() && declaration() == variable;
    }
}
