#pragma once

#include <memory>

#include "ir/flow.h"
#include "ir/scope.h"

namespace toycc::ir {
    class PostProcessor {
        public:
            static std::shared_ptr<Scope> process(std::shared_ptr<Scope> global_scope);

        private:
            // -------- Exported operations -> ir/postprocessor/exports.cpp
            PostProcessor(std::shared_ptr<Scope> global_scope);
            std::shared_ptr<Scope> operator() ();

            std::shared_ptr<Scope> global_scope;
            size_t unique_id = 0;
            std::shared_ptr<Type> offset_type;

            // -------- Split indirection levels -> ir/postprocessor/indirection.cpp
            // After this, multi-index dereferences are from a single pointer
            void split_indirections(std::shared_ptr<Scope> scope);
            Operand split_operand_indirections(Operand operand, std::shared_ptr<Scope> scope);

            // -------- Convert all types to raw storage types -> ir/postprocessor/detype.cpp
            void detype(std::shared_ptr<Scope> scope);
            void detype_operand(Operand& operand);

            // -------- Flatten all functions -> ir/postprocessor/descope.cpp
            void descope(std::shared_ptr<Scope> scope);

            // -------- Flatten multi-level and dynamic indexing -> ir/postprocessor/dereference.cpp
            void dereference(std::shared_ptr<Scope> scope);
            Operand dereference_operand(Operand original, std::shared_ptr<Scope> scope);
            Operand fully_dereference_operand(Operand original, std::shared_ptr<Scope> scope);
            Operand make_offset(std::shared_ptr<Type> pointer_type, Operand index, std::shared_ptr<Scope> scope);
            Operand make_pointer_offset(std::shared_ptr<Type> pointer_type, Operand index, std::shared_ptr<Scope> scope);
            Operand make_struct_offset(std::shared_ptr<Type> pointer_type, Operand index, std::shared_ptr<Scope> scope);
            Operand make_union_offset(std::shared_ptr<Type> pointer_type, Operand index, std::shared_ptr<Scope> scope);
            Operand merge_offsets(Operand flat_offset, Operand offset, std::shared_ptr<Scope> scope);

            // -------- Control flow analysis -> ir/postprocessor/flow_analysis.cpp
            TranslationUnit analyse_flow(std::shared_ptr<Scope> global_scope);

            // -------- State management -> ir/postprocessor/state.cpp
            std::string anonymous_identifier();
            std::string anonymous_label();
            std::string anonymous_type();
            std::string make_scope_prefix();
            std::string make_scope_prefix(std::string name);
            std::shared_ptr<Declaration> declare_temporary(std::shared_ptr<Scope> scope, std::shared_ptr<Type> type, CodeLocation location);
    };
}
