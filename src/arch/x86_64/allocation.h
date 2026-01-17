#pragma once

#include <string>

#include "ir/flow.h"
#include "ir/allocation.h"
#include "arch/x86_64/output.h"
#include "gen/execmodel/x86_64/location.h"

namespace toycc::arch::x86_64 {
    using toycc::execmodel::x86_64::Location;

    using Allocation = ir::Allocation<Location>;

    // Stack frame object that automatically generates its frame push and pop code
    struct StackFrame : public ir::StackFrame<Location> {
        public:
            StackFrame(const ir::Procedure& procedure);

            std::unordered_set<Location> locate(const ir::Operand& operand) const;
            std::optional<Location> allocate(const std::unordered_set<Location>& locations) const;
            std::string str() const;

            std::shared_ptr<ir::Declaration> declare_intermediate(std::shared_ptr<ir::Type> type, CodeLocation code_location);
            void flush_intermediates();

            std::string dump() const;

            CodeOutput output;

        private:
            std::string name;
            size_t unique_id = 0;
            std::unordered_set<std::shared_ptr<ir::Declaration>> intermediates;
    };

    CodeOutput& operator<< (CodeOutput& output, const StackFrame& code);
}
