#pragma once

#include <array>
#include <memory>
#include <optional>
#include <unordered_set>

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

    struct GroupMatch {
       TranslationGroupTag group;
       std::vector<std::shared_ptr<ir::DependencyNode>> statements;
       std::unordered_set<std::shared_ptr<ir::DependencyNode>> link_values;
    };

    std::vector<GroupMatch> match_dependency_subgraph(TranslationGroupTag group, const ir::DependencyMatrix& graph, const std::vector<ir::StatementTag>& statements, const arma::imat& subgraph);

    struct OperandMatch {
        enum MatchResult {OK, REQUIRES_TRANSFER, KO};

        MatchResult match;
        std::optional<Location> location;  // OK -> location to use ; REQUIRES_TRANSFER -> where it should go

        inline OperandMatch() = default;
        inline OperandMatch(MatchResult result) : match(result) {}
        inline OperandMatch(MatchResult result, Location location) : match(result), location(location) {}
        inline OperandMatch(MatchResult result, std::optional<Location> location) : match(result), location(location) {}
    };

    struct StatementMatch {
        std::vector<OperandMatch> input;
        std::optional<OperandMatch> output;

        inline bool matches() const {
            for (const OperandMatch& operand : input)
                if (operand.match != OperandMatch::OK)
                    return false;
            if (output.has_value() && output->match != OperandMatch::OK)
                return false;
            return true;
        }

        // Return the number of transfers required to fix this match, or empty optional if it's not fixable
        inline std::optional<size_t> nof_transfers() const {
            size_t result = 0;
            for (const OperandMatch& operand : input) {
                if (operand.match == OperandMatch::REQUIRES_TRANSFER)
                    result += 1;
                else if (operand.match != OperandMatch::OK)
                    return {};
            }

            if (output.has_value()) {
                if (output->match == OperandMatch::REQUIRES_TRANSFER)
                    result += 1;
                else if (output->match != OperandMatch::OK)
                    return {};
            }
            return result;
        }
    };

    struct TranslationMatch {
        TranslationTag translation;
        GroupMatch group_match;
        std::vector<StatementMatch> statements;
        std::vector<OperandMatch> allocations;

        inline bool matches() const {
            for (const StatementMatch& statement : statements)
                if (!statement.matches())
                    return false;
            for (const OperandMatch& allocation : allocations)
                if (allocation.match != OperandMatch::OK)
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

            for (const OperandMatch& allocation : allocations) {
                if (allocation.match == OperandMatch::KO)
                    return {};
                else if (allocation.match == OperandMatch::REQUIRES_TRANSFER)
                    result += 1;
            }

            return result;
        }
    };

    std::ostream& operator<< (std::ostream& stream, const GroupMatch& match);
    std::ostream& operator<< (std::ostream& stream, const OperandMatch& match);
    std::ostream& operator<< (std::ostream& stream, const StatementMatch& match);
    std::ostream& operator<< (std::ostream& stream, const TranslationMatch& match);

    std::string dump(const GroupMatch& match);
    std::string dump(const OperandMatch& match);
    std::string dump(const StatementMatch& match);
    std::string dump(const TranslationMatch& match);

    void update_translation_match(std::optional<TranslationMatch>& result, TranslationMatch&& match);
}
