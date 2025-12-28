#include "diagnostic.h"
#include "ir/generator.h"

namespace toycc::ir {
    Generator::ExpressionResult::ExpressionResult(CodeLocation location, std::shared_ptr<Declaration> result, bool is_lvalue, Generator& generator)
        : location(location), result(result), is_lvalue(is_lvalue), generator(generator) {}

    std::shared_ptr<Declaration> Generator::ExpressionResult::load(CodeLocation location) {
        if (indices.empty())
            return result;

        // Otherwise emit a dereference
        std::shared_ptr<Declaration> destination = generator.declare_temporary(type(), location);
        generator.current_scope()->add_statement(std::make_shared<stmt::DerefLoad>(location, destination, result, indices));
        return destination;
    }

    void Generator::ExpressionResult::store(std::shared_ptr<Declaration> source, CodeLocation location) {
        if (!is_lvalue)
            throw Diagnostic(DiagnosticLevel::ERROR, "Can't store into an rvalue", location);

        std::shared_ptr<Declaration> stored_value = generator.emit_implicit_conversion(type(), source, location);

        if (indices.empty())
            generator.current_scope()->add_statement(std::make_shared<stmt::Copy>(location, stmt::ConversionOperation::COPY, result, stored_value));
        else
            generator.current_scope()->add_statement(std::make_shared<stmt::DerefStore>(location, result, indices, stored_value));
    }

    Generator::ExpressionResult::~ExpressionResult() {
        if (!postfix_increments.empty())
            apply_postfix_operations();
    }

    void Generator::ExpressionResult::apply_postfix_operations() {
        // When the expression goes out of scope, apply the postfix operations
        // ++ and -- are only valid on pointer and integer lvalues
        if (!is_lvalue)
            throw Diagnostic(DiagnosticLevel::ERROR, "Postfix increment and decrement operations are only available on lvalues", location);

        if (result->spec.is_pointer_type()) {
            return apply_pointer_postfix_operations();
        } else if (result->spec.is_object_type() && result->spec.type->identifier.category == TypeCategory::PRIMITIVE) {
            const PrimitiveType& primitive = static_cast<const PrimitiveType&> (*result->spec.type);
            if (primitive.semantic == PrimitiveSemantic::INTEGER)
                apply_integer_postfix_operations();
            else throw Diagnostic(DiagnosticLevel::ERROR, "Postfix increment and decrement operations are only available on integer and pointer lvalues", location);
        } else throw Diagnostic(DiagnosticLevel::ERROR, "Postfix increment and decrement operations are only available on integer and pointer lvalues", location);
    }

    void Generator::ExpressionResult::apply_pointer_postfix_operations() {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Pointer arithmetic is not implemented", location);
    }

    void Generator::ExpressionResult::apply_integer_postfix_operations() {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Integer postfix operations are not implemented", location);
    }

    ir::TypeSpecification Generator::ExpressionResult::type() const {
        ir::TypeSpecification spec = result->spec;
        for (std::shared_ptr<Declaration> index : indices)
            spec = spec.referenced_type();
        return spec;
    }

    // Wrap a simple declaration into an ExpressionResult
    std::shared_ptr<Generator::ExpressionResult> Generator::make_expression(std::shared_ptr<Declaration> declaration, bool is_lvalue) {
        return std::make_shared<ExpressionResult>(declaration->location, declaration, is_lvalue, *this);
    }
}
