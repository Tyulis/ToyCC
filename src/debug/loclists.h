#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

#include "debug/dwarf.h"
#include "debug/encoder.h"

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
            size_t add(const LocationList& loclist);  // Add a location list, return its index
            size_t base() const;                      // Base offset of the offset table, for DW_AT_loclists_base
            void emit(CodeOutput& output) const;

        private:
            DWARFFormat format;
            std::vector<LocationList> loclists;
    };
}
