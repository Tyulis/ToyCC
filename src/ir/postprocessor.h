#pragma once

#include <memory>
#include "ir/type.h"
#include "ir/scope.h"
#include "ir/type_expressions.h"

namespace toycc::ir {
    class PostProcessor {
        public:
            // -------- Exported operations -> ir/postprocessor/exports.cpp
            PostProcessor(std::shared_ptr<Scope> global_scope);
            std::shared_ptr<Scope> operator() ();

        private:
            std::shared_ptr<Scope> global_scope;
            size_t unique_id = 0;

            // -------- Convert all types to raw storage types -> ir/postprocessor/detype.cpp
            std::shared_ptr<Type> pointer_storage_type;

            void detype(std::shared_ptr<Scope> scope);
            std::shared_ptr<Type> to_storage_type(std::shared_ptr<Type> type);
            std::shared_ptr<Type> to_array_storage_type(std::shared_ptr<ArrayType> type);
            std::shared_ptr<Type> to_compound_storage_type(std::shared_ptr<CompoundType> type);
            std::shared_ptr<Type> to_function_storage_type(std::shared_ptr<FunctionType> type);
            std::shared_ptr<Type> to_bitfield_storage_type(std::shared_ptr<BitfieldType> type);
            std::shared_ptr<Type> to_aligned_storage_type(std::shared_ptr<AlignedType> type);
            std::shared_ptr<Type> to_qualified_storage_type(std::shared_ptr<QualifiedType> type);

            // -------- Flatten all functions -> ir/postprocessor/descope.cpp
            void descope(std::shared_ptr<Scope> scope);

            // -------- State management -> ir/postprocessor/state.cpp
            std::string anonymous_identifier();
            std::string anonymous_label();
            std::string anonymous_type();
            std::string make_scope_prefix();
            std::string make_scope_prefix(std::string name);
    };
}
