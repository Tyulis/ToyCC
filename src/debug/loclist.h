#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

#include "debug/dwarf.h"
#include "debug/encoder.h"
#include "ir/declaration.h"

namespace toycc::debug {
    struct LocationRange {
        AssemblyData location;
        std::optional<std::string> start_label;
        std::optional<std::string> end_label;
    };

    class LocationList {
        public:
            void copy(const AssemblyData& location, const std::string& label);
            void move(const AssemblyData& location, const std::string& label);
            void free(const std::string& label);
            void set_default(const AssemblyData& location);
            Encoder& emit(Encoder& encoder) const;

        private:
            std::vector<LocationRange> ranges;
            std::unordered_map<AssemblyData, LocationRange> current_locations;
            std::optional<AssemblyData> default_location;
    };

    class LocationListsSection {
        public:
            LocationListsSection(DWARFFormat format);
            size_t index(std::shared_ptr<ir::Declaration> declaration);
            void copy(std::shared_ptr<ir::Declaration> declaration, const AssemblyData& location, const std::string& label);
            void move(std::shared_ptr<ir::Declaration> declaration, const AssemblyData& location, const std::string& label);
            void free(std::shared_ptr<ir::Declaration> declaration, const std::string& label);
            void set_default(std::shared_ptr<ir::Declaration> declaration, const AssemblyData& location);
            std::string str() const;

            size_t base() const;  // Base offset of the offset table, for DW_AT_loclists_base

        private:
            DWARFFormat format;
            std::vector<std::shared_ptr<ir::Declaration>> order;
            std::unordered_map<std::shared_ptr<ir::Declaration>, LocationList> lists;

            LocationList& get(std::shared_ptr<ir::Declaration> declaration);
    };
}
