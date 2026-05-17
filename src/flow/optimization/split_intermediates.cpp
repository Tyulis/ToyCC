#include "diagnostic.h"
#include "flow/block.h"
#include "flow/procedure.h"
#include "flow/unit.h"

namespace toycc::flow {
    static void replace_declaration(ir::Operand& operand, std::shared_ptr<ir::Declaration> initial, std::shared_ptr<ir::Declaration> replacement) {
        if (operand.has_variable_base() && operand.declaration() == initial)
            operand.value = replacement;
    }

    // Make intermediate values of external variables into internal temporaries
    void BasicBlock::opt_split_intermediates() {
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
                *value_node = DependencyNode {intermediate};

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

    void Procedure::opt_split_intermediates() {
        for (std::shared_ptr<BasicBlock> block : blocks.nodes())
            block->opt_split_intermediates();
    }

    void TranslationUnit::opt_split_intermediates() {
        for (auto& [name, procedure] : procedures)
            procedure.opt_split_intermediates();
    }
}
