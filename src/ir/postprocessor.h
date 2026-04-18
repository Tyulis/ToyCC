#pragma once

#include <memory>

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

            // -------- Split indirection levels -> ir/postprocessor/indirection.cpp
            // After this, multi-index dereferences are from a single pointer
            void split_indirections(std::shared_ptr<Scope> scope);
            Operand split_operand_indirections(Operand operand, std::shared_ptr<Scope> scope);

            // -------- Access block types (struct, array, ...) using pointers to members -> ir/postprocessor/blocks.cpp
            void split_blocks(std::shared_ptr<Scope> scope);
            Operand split_operand_blocks(Operand operand, std::shared_ptr<Scope> scope);

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

            // -------- Extract string literals as global constants -> ir/postprocessor/strings.cpp
            void extract_strings(std::shared_ptr<Scope> global_scope);
            std::unordered_map<std::shared_ptr<Declaration>, std::string> extract_string_literals(std::shared_ptr<Scope> scope);
            void extract_string_literals(std::unordered_map<std::shared_ptr<Declaration>, std::string>& literals, Operand& operand);

            // -------- State management -> ir/postprocessor/state.cpp
            std::string anonymous_identifier();
            std::string anonymous_label();
            std::string make_scope_prefix();
            std::string make_scope_prefix(std::string name);
            std::shared_ptr<Declaration> declare_temporary(std::shared_ptr<Scope> scope, std::shared_ptr<Type> type, CodeLocation location);
    };
}
