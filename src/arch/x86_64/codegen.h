#pragma once

#include "ir/flow.h"
#include "arch/codegen.h"
#include "arch/x86_64/output.h"
#include "arch/x86_64/allocation.h"

namespace toycc::arch::x86_64 {
    using namespace toycc::ir;

    class CodeGenerator : public toycc::arch::CodeGenerator {
        public:
            // -------- Exported methods -> arch/x86_64/exports.cpp
            CodeGenerator(const TranslationUnit& unit);
            virtual void operator() (std::ostream& output) override;

        private:
            // -------- Global constructs -> arch/x86_64/global.cpp
            void generate_translation_unit(CodeOutput& output, const TranslationUnit& unit);
            void generate_procedure(CodeOutput& output, const Procedure& procedure, const std::unordered_set<std::shared_ptr<Declaration>>& globals);

            // -------- Basic block generation -> arch/x86_64/block.cpp
            void generate_basic_block(StackFrame& frame, std::shared_ptr<BasicBlock> block, const std::unordered_set<std::shared_ptr<Declaration>>& globals);
            void generate_iteration(StackFrame& frame, DependencyGraph& graph, std::shared_ptr<BasicBlock> block, const std::unordered_set<std::shared_ptr<Declaration>>& globals);
            std::vector<DependencyGraph> entry_statement_subgraphs(const DependencyGraph& graph, size_t max_depth) const;

            // -------- Statements -> arch/x86_64/statements.cpp
            void generate_statement(StackFrame& frame, Statement& statement);
            void generate_return(StackFrame& frame, const Statement& statement);

            // -------- Operand management -> arch/x86_64/operands.cpp
            OperandLocation move_operands(StackFrame& frame, Statement& statement);
            LOC move_operand(StackFrame& frame, Operand& operand, Flags<LOC> allowed_locations, CodeLocation code_location);
            LOC clear_output(StackFrame& frame, const OperandLocation& operands, Operand& operand, Flags<LOC> allowed_locations, CodeLocation code_location);

            std::string operand_ref(StackFrame& frame, const Operand& operand, CodeLocation code_location) const;
            std::string variable_ref(StackFrame& frame, std::shared_ptr<Declaration> declaration, CodeLocation code_location) const;
            std::string variable_ref(StackFrame& frame, std::shared_ptr<Declaration> declaration, LOC location, CodeLocation code_location) const;
            std::optional<std::string> register_ref(LOC location, size_t size) const;

            // -------- Data movement -> arch/x86_64/movement.cpp
            std::shared_ptr<Declaration> load_constant(StackFrame& frame, const Constant& constant, LOC destination, CodeLocation code_location);
            void move_variable(StackFrame& frame, std::shared_ptr<Declaration> variable, LOC destination, CodeLocation code_location);

            // -------- Symbol management -> arch/x86_64/symbols.cpp
            size_t unique_id = 0;
            std::string anonymous_identifier();
    };
}
