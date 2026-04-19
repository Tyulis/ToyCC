#pragma once

#include <memory>
#include <cstddef>
#include <unordered_set>
#include <unordered_map>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>

#include "ir/flow.h"
#include "ir/declaration.h"
#include "debug/unit.h"
#include "util/alignment.hpp"

namespace toycc::ir {
    using namespace boost::multi_index;

    template <typename Location>
    struct Allocation {
        std::shared_ptr<Declaration> declaration;
        Location location;
    };

    struct location_tag {};
    struct declaration_tag {};

    template <typename Location>
    using AllocationTable = multi_index_container<Allocation<Location>,
        indexed_by<hashed_non_unique<tag<location_tag>,    member<Allocation<Location>, Location,                     &Allocation<Location>::location>>,
                   hashed_non_unique<tag<declaration_tag>, member<Allocation<Location>, std::shared_ptr<Declaration>, &Allocation<Location>::declaration>>>>;

    template <typename Location>
    class StackFrame {
        public:
            const Procedure& procedure;

            StackFrame(const Procedure& procedure, const std::unordered_set<Location>& nonunique_locations) : procedure(procedure), nonunique_locations(nonunique_locations) {}

            // -------- Stack variables management
            // Get the offset of the requested variable in the stack frame.
            // If the variable is not yet in the stack frame, add it
            size_t offset(std::shared_ptr<Declaration> declaration) {
                auto it = stack_offsets.find(declaration);
                if (it == stack_offsets.end()) {
                    const size_t result = align_offset(current_offset, declaration->type->size({}));
                    stack_offsets[declaration] = result;
                    current_offset = result + declaration->type->size(declaration->location);
                    return result;
                } else {
                    return it->second;
                }
            }


            // -------- Value allocation management
            std::unordered_set<Location> locate(std::shared_ptr<Declaration> declaration) const {
                const auto& declaration_index = allocations.template get<declaration_tag>();
                std::unordered_set<Location> locations;
                for (auto [begin, end] = declaration_index.equal_range(declaration); begin != end; begin++)
                    locations.insert(begin->location);
                return locations;
            }

            std::shared_ptr<Declaration> content(Location location) const {
                const auto& location_index = allocations.template get<location_tag>();
                auto it = location_index.find(location);
                if (it == location_index.end())  return nullptr;
                else                             return it->declaration;
            }

            // Remove all existing locations of this variable and move it elsewhere. If there is something at `location`, it is overwritten
            void move(std::shared_ptr<Declaration> declaration, Location location) {
                auto& declaration_index = allocations.template get<declaration_tag>();
                auto& location_index = allocations.template get<location_tag>();

                declaration_index.erase(declaration);
                if (!nonunique_locations.contains(location))
                    location_index.erase(location);

                declaration_index.emplace(declaration, location);
                used_locations.insert(location);

                move_debug_variable(declaration);
            }

            // Add another location for a variable. If there is already something at `location`, it is overwritten
            void copy(std::shared_ptr<Declaration> declaration, Location location) {
                auto& location_index = allocations.template get<location_tag>();
                if (!nonunique_locations.contains(location)) {
                    location_index.erase(location);
                } else {
                    // Don't reinsert the allocation if it already exists with the same variable and location
                    auto& declaration_index = allocations.template get<declaration_tag>();
                    for (auto [it, end] = declaration_index.equal_range(declaration); it != end; it++)
                        if (it->location == location)
                            return;
                }

                location_index.emplace(declaration, location);
                used_locations.insert(location);

                move_debug_variable(declaration);
            }

            // Remove all locations of this variable
            void free(std::shared_ptr<Declaration> declaration) {
                auto& declaration_index = allocations.template get<declaration_tag>();
                declaration_index.erase(declaration);
            }


            // -------- Debug info
            void emit_debuginfo(debug::CompilationUnit& debuginfo) const {
                for (std::shared_ptr<Declaration> declaration : debug_variables)
                    debuginfo.append(debuginfo.variable(declaration));
            }

        protected:
            const std::unordered_set<Location> nonunique_locations;

            std::unordered_map<std::shared_ptr<Declaration>, size_t> stack_offsets;
            size_t current_offset = 0;

            AllocationTable<Location> allocations;
            std::unordered_set<Location> used_locations;

            std::unordered_set<std::shared_ptr<Declaration>> debug_variables;

            void move_debug_variable(std::shared_ptr<Declaration> declaration) {
                if (declaration->storage & (StorageClass::TEMPORARY | StorageClass::GLOBAL))  // FIXME : What to do with global variables ?
                    return;  // Not from source code -> nothing to do in debug info

                debug_variables.insert(declaration);
            }
    };
}
