#pragma once

#include <array>
#include <string>
#include <optional>
#include <unordered_set>
#include <unordered_map>

#include "gen/execmodel/x86_64/location.h"

namespace toycc::arch::x86_64 {
    using toycc::execmodel::x86_64::Location;
    constexpr std::array<Location, 6> INTEGER_REGISTER_ARGUMENTS = {Location::di, Location::si, Location::d, Location::c, Location::r8, Location::r9};
    constexpr std::array<Location, 8> FLOAT_REGISTER_ARGUMENTS   = {Location::mm0, Location::mm1, Location::mm2, Location::mm3,
                                                                    Location::mm4, Location::mm5, Location::mm6, Location::mm7};

    extern const std::unordered_set<Location> CALLER_SAVED;
    extern const std::unordered_set<Location> CALLEE_SAVED;
    extern const std::unordered_map<Location, std::unordered_map<size_t, std::string>> REGISTER_NAMES;

    std::optional<Location> best_location(std::unordered_set<Location> available_locations);
}
