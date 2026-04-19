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
            LocationList(DWARFFormat format);
            void set  (const AssemblyData& location, const std::string& label);
            void unset(const AssemblyData& location, const std::string& label);
            void end  (const std::string& label);
            AssemblyData encode() const;

        private:
            DWARFFormat format;
            std::vector<LocationRange> ranges;
            std::unordered_map<AssemblyData, LocationRange> current_locations;
    };
}
