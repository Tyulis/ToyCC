#include "debug/dwarf.h"
#include "debug/loclist.h"

namespace toycc::debug {
    LocationList& LocationList::end() {
        int8(LocationListEntryType::DW_LLE_end_of_list);
        return *this;
    }

    Encoder& operator<< (Encoder& encoder, const LocationList& loclist) {
        return encoder.insert(loclist.str(), loclist.length());
    }
}
