#include <limits>
#include <concepts>
#include <functional>

#include "diagnostic.h"
#include "flow/block.h"
#include "flow/dependencies.h"
#include "flow/procedure.h"
#include "flow/unit.h"
#include "ir/declaration.h"
#include "util/sets.hpp"
#include "util/strings.h"

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
            if (input.base_tag() == ir::Operand::VARIABLE_BASE && input.declaration() == value_node->declaration())
                input = ir::Operand {value_node->value().value(), input.location};

        if (statement.output.has_value() && statement.output->tag() == ir::Operand::DEREFERENCE && statement.output->base_tag() == ir::Operand::VARIABLE_BASE && statement.output->declaration() == value_node->declaration())
            statement.output = ir::Operand {value_node->value().value(), statement.output->location};

        // Now that all instances of this value node have been replaced, the statement doesn't depend on the actual variable anymore
        std::optional<DependencyGraph::Edge> edge = graph.find_edge(value_node, statement_node);
        if (edge.has_value()) {
            edge->attr.type.clear(DependencyType::READ);
            edge->attr.type.clear(DependencyType::WRITE);
            if (edge->attr.type.empty())
                graph.pop_edge(edge.value());  // No remaining dependency -> completely disconnect the nodes
            else
                graph.add_edge(edge.value());  // Just delete the direct dependencies, the value may still have indirect dependencies or be live on exit
        }

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
    struct left_shift {
        ir::IntegerConstant operator() (ir::IntegerConstant operand, ir::IntegerConstant shift) const {
            return operand << static_cast<size_t>(shift);
        };
    };

    struct arithmetic_right_shift {
        ir::IntegerConstant operator() (ir::IntegerConstant operand, ir::IntegerConstant shift) const {
            // Arithmetic right shift is equivalent to division by 2^shift, rounded towards negative infinity
            // Regular division is rounded towards zero, the divisor is always positive
            ir::IntegerConstant divisor = (ir::IntegerConstant(1) << static_cast<size_t>(shift));
            ir::IntegerConstant quotient = operand / divisor;
            ir::IntegerConstant modulus  = operand % divisor;
            if (operand < 0 && modulus != 0)
                quotient -= 1;
            return quotient;
        }
    };

    struct logical_right_shift {
        ir::IntegerConstant operator() (ir::IntegerConstant operand, ir::IntegerConstant shift) const {
            if (operand >= 0)
                return operand >> static_cast<size_t>(shift);

            // Logical right shift of a negative number is a weird case, realistically it shouldn't happen, but just in case :
            ir::IntegerConstant twos_complement_value = std::numeric_limits<ir::IntegerConstant>::max() + operand + 1;
            return twos_complement_value >> static_cast<size_t>(shift);
        }
    };

    std::shared_ptr<DependencyNode> output_node(std::shared_ptr<DependencyNode> statement_node, const DependencyGraph& graph) {
        for (const DependencyGraph::Edge& edge : graph.out_edges(statement_node))
            if (edge.attr.type & DependencyType::WRITE)
                return edge.exit;
        return nullptr;
    }

    void set_output_value(std::shared_ptr<DependencyNode> statement_node, DependencyGraph& graph, const ir::Constant& value) {
        std::shared_ptr<DependencyNode> value_node = output_node(statement_node, graph);
        if (value_node.get() != nullptr)
            value_node->value() = value;
    }

    // Check whether all inputs are constants
    bool is_constant_statement(std::shared_ptr<DependencyNode> statement_node) {
        return std::ranges::all_of(statement_node->statement().inputs, [](const ir::Operand& input) { return input.tag() == ir::Operand::CONSTANT; });
    }

    // Check whether all inputs are constants, and the output is a variable
    bool is_constants_to_variable(std::shared_ptr<DependencyNode> statement_node) {
        return is_constant_statement(statement_node) && statement_node->statement().output->tag() == ir::Operand::VARIABLE;
    }

    // Set variables to the `value` directly, or replace the more complex statement with a COPY if the output is a dereference. Return whether to keep the statement or not
    bool set_copy(std::shared_ptr<DependencyNode> statement_node, DependencyGraph& graph, const ir::Constant& value) {
        const ir::Statement& original_statement = statement_node->statement();

        bool emit_copy = true;
        if (original_statement.output->tag() == ir::Operand::VARIABLE) {
            set_output_value(statement_node, graph, value);
            std::shared_ptr<DependencyNode> value_node = output_node(statement_node, graph);

            if (value_node.get() == nullptr)
                emit_copy = false;
            else  // If the value is live on exit or may be used through a dereference, it still needs to be explicitely copied to the variable even though it won't be used in this block
                emit_copy = graph.is_sink(value_node) ||
                            std::ranges::any_of(graph.out_edges(value_node), [](const DependencyGraph::Edge& edge) {
                                return edge.attr.type & (DependencyType::LIVE_ON_EXIT | DependencyType::DEREFERENCE | DependencyType::CALL);
                            });
        }

        if (emit_copy)
            statement_node->statement() = ir::Statement::make_unary_operation(original_statement.location, ir::StatementTag::COPY, value, original_statement.output.value());

        return emit_copy;
    }

    // Evaluate an integral unary operation on a constant, using objects like `std::negate`
    template <typename T> requires std::invocable<T, ir::IntegerConstant>
    ir::Constant evaluate_integral(const ir::Constant& input, std::shared_ptr<ir::Type> output_type, const CodeLocation location, T op = {}) {
        const ir::IntegerConstant result = op(input.integer());
        if (output_type->is_integral())
            return ir::Constant {result, location, output_type};
        else if (output_type->is_floating_point())
            return ir::Constant {ir::FloatingPointConstant(result), location, output_type};
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Invalid result type `{}` for this operation", output_type->repr()), location);
    }

    // Evaluate an integral binary operation between constants, using objects like `std::plus`
    template <typename T> requires std::invocable<T, ir::IntegerConstant, ir::IntegerConstant>
    ir::Constant evaluate_integral(const ir::Constant& left, const ir::Constant& right, std::shared_ptr<ir::Type> output_type, const CodeLocation location, T op = {}) {
        const ir::IntegerConstant result = op(left.integer(), right.integer());
        if (output_type->is_integral())
            return ir::Constant {result, location, output_type};
        else if (output_type->is_floating_point())
            return ir::Constant {ir::FloatingPointConstant(result), location, output_type};
        else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Invalid result type `{}` for this operation", output_type->repr()), location);
    }

    // Evaluate a unary integer operation statement with constant input
    template <typename T> requires std::invocable<T, ir::IntegerConstant>
    bool evaluate_integral_statement(std::shared_ptr<DependencyNode> statement_node, DependencyGraph& graph, T op = {}) {
        if (!is_constant_statement(statement_node))
            return true;

        const ir::Statement& statement = statement_node->statement();
        const ir::Constant result = evaluate_integral(statement.inputs[0].constant(), statement.output->type(), statement.output->location, op);
        return set_copy(statement_node, graph, result);
    }

    // Evaluate a binary integer operation statement with all constant inputs
    template <typename T> requires std::invocable<T, ir::IntegerConstant, ir::IntegerConstant>
    bool evaluate_integral_statement(std::shared_ptr<DependencyNode> statement_node, DependencyGraph& graph, T op = {}) {
        if (!is_constant_statement(statement_node))
            return true;

        const ir::Statement& statement = statement_node->statement();
        const ir::Constant result = evaluate_integral(statement.inputs[0].constant(), statement.inputs[1].constant(), statement.output->type(),
                                                        statement.output->location, op);
        return set_copy(statement_node, graph, result);
    }

    // Evaluate an arithmetic unary operation on a constant, using objects like `std::negate`
    template <typename T> requires std::invocable<T, ir::IntegerConstant> && std::invocable<T, ir::FloatingPointConstant>
    ir::Constant evaluate_arithmetic(const ir::Constant& input, std::shared_ptr<ir::Type> output_type, const CodeLocation location, T op = {}) {
        if (input.tag() == ir::Constant::STRING) {
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "This operator is not applicable to strings", location);
        } else if (input.tag() == ir::Constant::INTEGER) {
            return evaluate_integral(input, output_type, location, op);
        } else {
            ir::FloatingPointConstant result;
            if (input.tag() == ir::Constant::INTEGER)
                result = ir::FloatingPointConstant(op(input.integer()));
            else
                result = op(input.floating_point());

            if (output_type->is_integral())
                return ir::Constant {ir::IntegerConstant(result), location, output_type};
            else if (output_type->is_floating_point())
                return ir::Constant {result, location, output_type};
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Invalid result type `{}` for this operation", output_type->repr()), location);
        }
    }

    // Evaluate an arithmetic binary operation between constants, using objects like `std::plus`
    template <typename T> requires std::invocable<T, ir::IntegerConstant, ir::IntegerConstant> && std::invocable<T, ir::FloatingPointConstant, ir::FloatingPointConstant>
    ir::Constant evaluate_arithmetic(const ir::Constant& left, const ir::Constant& right, std::shared_ptr<ir::Type> output_type, const CodeLocation location, T op = {}) {
        if (left.tag() == ir::Constant::STRING || right.tag() == ir::Constant::STRING) {
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "This operator is not applicable to strings", location);
        }

        else if (left.tag() == ir::Constant::INTEGER && right.tag() == ir::Constant::INTEGER) {
            return evaluate_integral(left, right, output_type, location, op);
        }

        else {
            ir::FloatingPointConstant result;
            if (left.tag() == ir::Constant::INTEGER && right.tag() == ir::Constant::FLOAT)
                result = op(ir::FloatingPointConstant(left.integer()), right.floating_point());
            else if (left.tag() == ir::Constant::FLOAT && right.tag() == ir::Constant::INTEGER)
                result = op(left.floating_point(), ir::FloatingPointConstant(right.integer()));
            else if (left.tag() == ir::Constant::FLOAT && right.tag() == ir::Constant::FLOAT)
                result = op(left.floating_point(), right.floating_point());
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Invalid combination of constant tags", location);

            if (output_type->is_integral())
                return ir::Constant {ir::IntegerConstant(result), location, output_type};
            else if (output_type->is_floating_point())
                return ir::Constant {result, location, output_type};
            else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Invalid result type `{}` for this operation", output_type->repr()), location);
        }
    }

    // Evaluate a unary arithmetic operation statement with constant input
    template <typename T> requires std::invocable<T, ir::IntegerConstant> && std::invocable<T, ir::FloatingPointConstant>
    bool evaluate_arithmetic_statement(std::shared_ptr<DependencyNode> statement_node, DependencyGraph& graph, T op = {}) {
        if (!is_constant_statement(statement_node))
            return true;

        const ir::Statement& statement = statement_node->statement();
        const ir::Constant result = evaluate_arithmetic(statement.inputs[0].constant(), statement.output->type(), statement.output->location, op);
        return set_copy(statement_node, graph, result);
    }

    // Evaluate a binary arithmetic operation statement with all constant inputs
    template <typename T> requires std::invocable<T, ir::IntegerConstant, ir::IntegerConstant> && std::invocable<T, ir::FloatingPointConstant, ir::FloatingPointConstant>
    bool evaluate_arithmetic_statement(std::shared_ptr<DependencyNode> statement_node, DependencyGraph& graph, T op = {}) {
        if (!is_constant_statement(statement_node))
            return true;

        const ir::Statement& statement = statement_node->statement();
        const ir::Constant result = evaluate_arithmetic(statement.inputs[0].constant(), statement.inputs[1].constant(), statement.output->type(),
                                                        statement.output->location, op);
        return set_copy(statement_node, graph, result);
    }

    bool evaluate_copy(std::shared_ptr<DependencyNode> statement_node, DependencyGraph& graph) {
        if (is_constant_statement(statement_node))
            return set_copy(statement_node, graph, statement_node->statement().inputs[0].constant());
        else
            return true;
    }

    bool evaluate_narrowing_conversion(std::shared_ptr<DependencyNode> statement_node, DependencyGraph& graph) {
        if (!is_constant_statement(statement_node))
            return true;

        const ir::Statement& statement = statement_node->statement();
        const ir::Constant& input = statement.inputs[0].constant();

        if (input.tag() != ir::Constant::INTEGER)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "NARROW is only valid on integer values", input.location);
        if (statement.output->type()->category != ir::TypeCategory::INTEGER)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "NARROW only converts to integer types", statement.output->location);

        std::shared_ptr<ir::IntegerType> output_type = std::static_pointer_cast<ir::IntegerType>(statement.output->type());
        const ir::IntegerConstant mask = (ir::IntegerConstant(1) << output_type->size_bits) - 1;
        const ir::Constant result = {input.integer() & mask, statement.output->location, statement.output->type()};
        return set_copy(statement_node, graph, result);
    }

    // Attempt to evaluate a constant expression statement, return whether the statement must be kept
    static bool evaluate_statement(std::shared_ptr<DependencyNode> statement_node, DependencyGraph& graph) {
        const ir::Statement& statement = statement_node->statement();

        switch (statement.tag) {
            case ir::StatementTag::MARKER:
            case ir::StatementTag::BLOCK:
            case ir::StatementTag::FUNCTION:
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("`{}` statements should disappear before constant folding", dump(statement.tag)), statement.location);

            case ir::StatementTag::CALL:               return true;
            case ir::StatementTag::JUMP:               return true;
            case ir::StatementTag::JUMP_IF_TRUE:       return true;
            case ir::StatementTag::JUMP_IF_FALSE:      return true;
            case ir::StatementTag::RETURN:             return true;
            case ir::StatementTag::RETURN_VAL:         return true;
            case ir::StatementTag::COPY:               return evaluate_copy(statement_node, graph);
            case ir::StatementTag::NEGATE:             return evaluate_arithmetic_statement(statement_node, graph, std::negate {});
            case ir::StatementTag::NOT:                return evaluate_integral_statement(statement_node, graph, std::logical_not {});
            case ir::StatementTag::COMPLEMENT:         return evaluate_integral_statement(statement_node, graph, std::bit_not {});
            case ir::StatementTag::ADDRESSOF:          return true;  // TODO
            case ir::StatementTag::FLOAT_TO_FLOAT:     return evaluate_copy(statement_node, graph);  // Preserve the value but change size, that's irrelevant here
            case ir::StatementTag::INT_TO_FLOAT:       return evaluate_arithmetic_statement(statement_node, graph, std::identity {});
            case ir::StatementTag::FLOAT_TO_INT:       return evaluate_arithmetic_statement(statement_node, graph, std::identity {});
            case ir::StatementTag::SIGN_EXTEND:        return evaluate_copy(statement_node, graph);  // Sign- and zero- extensions are just ways to preserve the same value through length
            case ir::StatementTag::ZERO_EXTEND:        return evaluate_copy(statement_node, graph);  // extensions, outside of actual code generation they can all be evaluated as copies
            case ir::StatementTag::NARROW:             return evaluate_narrowing_conversion(statement_node, graph);
            case ir::StatementTag::MUL:                return evaluate_arithmetic_statement(statement_node, graph, std::multiplies {});
            case ir::StatementTag::DIV:                return evaluate_arithmetic_statement(statement_node, graph, std::divides {});
            case ir::StatementTag::MOD:                return evaluate_integral_statement(statement_node, graph, std::modulus {});
            case ir::StatementTag::ADD:                return evaluate_arithmetic_statement(statement_node, graph, std::plus {});
            case ir::StatementTag::SUB:                return evaluate_arithmetic_statement(statement_node, graph, std::minus {});
            case ir::StatementTag::LT:                 return evaluate_arithmetic_statement(statement_node, graph, std::less {});
            case ir::StatementTag::LE:                 return evaluate_arithmetic_statement(statement_node, graph, std::less_equal {});
            case ir::StatementTag::GE:                 return evaluate_arithmetic_statement(statement_node, graph, std::greater_equal {});
            case ir::StatementTag::GT:                 return evaluate_arithmetic_statement(statement_node, graph, std::greater {});
            case ir::StatementTag::EQ:                 return evaluate_arithmetic_statement(statement_node, graph, std::equal_to {});
            case ir::StatementTag::NE:                 return evaluate_arithmetic_statement(statement_node, graph, std::not_equal_to {});
            case ir::StatementTag::BITWISE_AND:        return evaluate_integral_statement(statement_node, graph, std::bit_and {});
            case ir::StatementTag::BITWISE_XOR:        return evaluate_integral_statement(statement_node, graph, std::bit_xor {});
            case ir::StatementTag::BITWISE_OR:         return evaluate_integral_statement(statement_node, graph, std::bit_or {});
            case ir::StatementTag::LSHIFT:             return evaluate_integral_statement(statement_node, graph, left_shift {});
            case ir::StatementTag::ARITHMETIC_RSHIFT:  return evaluate_integral_statement(statement_node, graph, arithmetic_right_shift {});
            case ir::StatementTag::LOGICAL_RSHIFT:     return evaluate_integral_statement(statement_node, graph, logical_right_shift {});
        }
        __builtin_unreachable();
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
