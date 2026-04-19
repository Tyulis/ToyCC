#include "diagnostic.h"
#include "debug/dwarf.h"
#include "debug/loclist.h"

namespace toycc::debug {
    LocationList::LocationList(DWARFFormat format) : format(format) {}

    void LocationList::set(const AssemblyData& location, const std::string& label) {
        if (current_locations.contains(location))
            return;  // Already in that location

        current_locations[location] = LocationRange {.location = location, .start_label = label, .end_label = {}};
    }

    void LocationList::unset(const AssemblyData& location, const std::string& label) {
        auto it = current_locations.find(location);
        if (it == current_locations.end())
            return;  // Not there, nothing to do

        it->second.end_label = label;
        ranges.push_back(it->second);  // Store the old location
        current_locations.erase(it);   // Then remove it from the variable's current locations
    }

    // Unset all locations with the given end `label`
    void LocationList::end(const std::string& label) {
        for (auto& [declaration, range] : current_locations) {
            range.end_label = label;
            ranges.push_back(range);
        }

        current_locations.clear();
    }

    AssemblyData LocationList::encode() const {
        if (!current_locations.empty())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to encode a location list with current locations");

        Encoder encoder(format);
        for (const LocationRange& range : ranges) {
            encoder.int8(LocationListEntryType::DW_LLE_start_end);
            encoder.address(range.start_label.value());
            encoder.address(range.end_label.value());
            encoder.insert(range.location);
        }

        encoder.int8(LocationListEntryType::DW_LLE_end_of_list);
        return encoder.encode();
    }
}
