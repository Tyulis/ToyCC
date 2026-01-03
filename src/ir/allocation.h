#pragma once

#include <memory>
#include <cstddef>
#include <unordered_map>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>

#include "ir/declaration.h"

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
        indexed_by<hashed_unique<tag<location_tag>,    member<Allocation<Location>, Location,                     &Allocation<Location>::location>>,
                   hashed_unique<tag<declaration_tag>, member<Allocation<Location>, std::shared_ptr<Declaration>, &Allocation<Location>::declaration>>>>;

    template <typename Location>
    struct StackFrame {
        // Stack variables management
        std::unordered_map<std::shared_ptr<Declaration>, size_t> locals;
        size_t current_position = 0;

        // Get the position of the requested variable in the stack frame.
        // If the variable is not yet in the stack frame, add it
        size_t position(std::shared_ptr<Declaration> declaration) {
            size_t position = current_position;
            current_position += declaration->type->size(declaration->location);

            locals[declaration] = position;
            return position;
        }

        // Value allocation management
        AllocationTable<Location> allocations;

        std::optional<Location> try_locate(std::shared_ptr<Declaration> declaration) const {
            const auto& declaration_index = allocations.template get<declaration_tag>();
            auto it = declaration_index.find(declaration);
            if (it == declaration_index.end())  return {};
            else                                return it->location;
        }

        std::shared_ptr<Declaration> try_content(Location location) const {
            const auto& location_index = allocations.template get<location_tag>();
            auto it = location_index.find(location);
            if (it == location_index.end())  return nullptr;
            else                             return it->declaration;
        }

        void move(std::shared_ptr<Declaration> declaration, Location new_location) {
            auto& declaration_index = allocations.template get<declaration_tag>();
            auto it = declaration_index.find(declaration);
            if (it == declaration_index.end())  declaration_index.insert (    Allocation<Location> {declaration, new_location});
            else                                declaration_index.replace(it, Allocation<Location> {declaration, new_location});
        }
    };
}
