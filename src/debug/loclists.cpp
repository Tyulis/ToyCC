#include <unordered_set>

#include "config.h"
#include "debug/settings.h"
#include "diagnostic.h"
#include "debug/encoder.h"
#include "debug/dwarf.h"
#include "debug/loclists.h"

namespace toycc::debug {
    // -------- LocationList
    void LocationList::copy(const AssemblyData& location, const std::string& label) {
        if (current_locations.contains(location))
            return;  // Already in that location

        current_locations[location] = LocationRange {.location = location, .start_label = label, .end_label = {}};
    }

    void LocationList::move(const AssemblyData& location, const std::string& label) {
        bool exists = false;
        std::unordered_set<AssemblyData> freed_locations;
        for (auto& [current_location, range] : current_locations) {
            if (current_location == location) {
                exists = true;
                continue;
            }

            range.end_label = label;
            ranges.push_back(range);
            freed_locations.insert(current_location);
        }

        for (const AssemblyData& current_location : freed_locations)
            current_locations.erase(current_location);

        if (!exists)
            current_locations[location] = LocationRange {.location = location, .start_label = label, .end_label = {}};
    }

    // Unset all locations with the given end `label`
    void LocationList::free(const std::string& label) {
        for (auto& [declaration, range] : current_locations) {
            range.end_label = label;
            ranges.push_back(range);
        }

        current_locations.clear();
    }

    // Unset one location at the given end `label`
    void LocationList::free_location(const AssemblyData& location, const std::string& label) {
        auto it = current_locations.find(location);
        if (it != current_locations.end()) {
            LocationRange& range = it->second;
            range.end_label = label;
            ranges.push_back(range);
            current_locations.erase(it);
        }
    }

    void LocationList::set_default(const AssemblyData& location) {
        default_location = location;
    }

    Encoder& LocationList::emit(Encoder& encoder) const {
        if (!current_locations.empty())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to encode a location list with current locations");

        for (const LocationRange& range : ranges) {
            if (range.start_label.value() == range.end_label.value())
                continue;  // Zero-length range, don't emit it

            encoder.int8(LocationListEntryType::DW_LLE_start_end);
            encoder.address(range.start_label.value());
            encoder.address(range.end_label.value());
            encoder.insert(range.location);
        }

        if (default_location.has_value()) {
            if (config::debug::with_default_location) {
                encoder.int8(LocationListEntryType::DW_LLE_default_location);
                encoder.insert(default_location.value());
            } else {
                // FIXME : If DW_LLE_default_location is unsupported, patch with the full range of .text ?
                encoder.int8(LocationListEntryType::DW_LLE_start_end);
                encoder.address(BEGIN_TEXT_LABEL);
                encoder.address(END_TEXT_LABEL);
                encoder.insert(default_location.value());
            }
        }

        encoder.int8(LocationListEntryType::DW_LLE_end_of_list);
        return encoder;
    }


    // -------- LocationListsSection
    LocationListsSection::LocationListsSection(DWARFFormat format) : format(format) {}

    size_t LocationListsSection::add(const LocationList& loclist) {
        const size_t index = loclists.size();
        loclists.push_back(loclist);
        return index;
    }

    size_t LocationListsSection::base() const {
        switch (format) {
            case DWARFFormat::DWARF32:  return 12;
            case DWARFFormat::DWARF64:  return 20;
        }
        __builtin_unreachable();
    }

    void LocationListsSection::emit(CodeOutput& output) const {
        LocationListHeader header;

        Encoder content(format);
        const size_t offset_table_size = loclists.size() * offset_size(format);
        for (const LocationList& loclist : loclists) {
            header.offsets.push_back(offset_table_size + content.length());
            loclist.emit(content);
        }

        LengthFieldEncoder section(format);
        section.header(header);
        section.insert(content.encode());
        output << section;
    }
}
