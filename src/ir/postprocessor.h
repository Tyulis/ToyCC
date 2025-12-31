#pragma once

#include <memory>
#include "ir/scope.h"

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
            void detype(std::shared_ptr<Scope> scope);

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
