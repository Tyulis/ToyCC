#pragma once

#include <array>
#include <memory>
#include <string>
#include <optional>
#include <unordered_set>
#include <unordered_map>

#include "ir/flow.h"
#include "gen/execmodel/x86_64/location.h"
#include "gen/execmodel/x86_64/translation_tag.h"

namespace toycc::arch::x86_64 {
    using toycc::execmodel::x86_64::Location;
    using toycc::execmodel::x86_64::TranslationTag;
    using toycc::execmodel::x86_64::TranslationGroupTag;

    constexpr std::array<Location, 6> INTEGER_REGISTER_ARGUMENTS = {Location::di, Location::si, Location::d, Location::c, Location::r8, Location::r9};
    constexpr std::array<Location, 8> FLOAT_REGISTER_ARGUMENTS   = {Location::mm0, Location::mm1, Location::mm2, Location::mm3,
                                                                    Location::mm4, Location::mm5, Location::mm6, Location::mm7};

    extern const std::unordered_set<Location> CALLER_SAVED;
    extern const std::unordered_set<Location> CALLEE_SAVED;
    extern const std::unordered_map<Location, std::unordered_map<size_t, std::string>> REGISTER_NAMES;

    struct GroupMatch {
       TranslationGroupTag group;
       std::vector<std::shared_ptr<ir::DependencyNode>> statements;
       std::unordered_set<std::shared_ptr<ir::DependencyNode>> link_values;
    };

    std::vector<GroupMatch> match_dependency_subgraph(TranslationGroupTag group, const ir::DependencyMatrix& graph, const std::vector<ir::StatementTag>& statements, const arma::imat& subgraph);

    enum class OperandMatch {
        OK, REQUIRES_TRANSFER, KO,
    };

    struct StatementMatch {
        std::vector<OperandMatch> input;
        std::optional<OperandMatch> output;

        inline bool matches() const {
            for (const OperandMatch& operand : input)
                if (operand != OperandMatch::OK)
                    return false;
            if (output.has_value() && *output != OperandMatch::OK)
                return false;
            return true;
        }

        // Return the number of transfers required to fix this match, or empty optional if it's not fixable
        inline std::optional<size_t> nof_transfers() const {
            size_t result = 0;
            for (const OperandMatch& operand : input) {
                if (operand == OperandMatch::REQUIRES_TRANSFER)
                    result += 1;
                else if (operand != OperandMatch::OK)
                    return {};
            }

            if (output.has_value()) {
                if (*output == OperandMatch::REQUIRES_TRANSFER)
                    result += 1;
                else if (*output != OperandMatch::OK)
                    return {};
            }
            return result;
        }
    };

    struct TranslationMatch {
        TranslationTag translation;
        GroupMatch group_match;
        std::vector<StatementMatch> statements;

        inline bool matches() const {
            for (const StatementMatch& statement : statements)
                if (!statement.matches())
                    return false;
            return true;
        }

        // Return the number of transfers required to fix this match, or empty optional if it's not fixable
        inline std::optional<size_t> nof_transfers() const {
            size_t result = 0;
            for (const StatementMatch& statement : statements) {
                std::optional<size_t> nof_transfers_statement = statement.nof_transfers();
                if (nof_transfers_statement.has_value())
                    result += nof_transfers_statement.value();
                else
                    return {};
            }
            return result;
        }
    };

    void update_translation_match(std::optional<TranslationMatch>& result, TranslationMatch&& match);

    std::optional<Location> best_location(std::unordered_set<Location> available_locations);
}
