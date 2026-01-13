#include <utility>
#include "arch/x86_64/execmodel.h"
#include "util/combinatorics.hpp"

namespace toycc::arch::x86_64 {
    const std::unordered_set<Location> CALLER_SAVED = {Location::a, Location::c, Location::d, Location::si, Location::di, Location::r8, Location::r9, Location::r10, Location::r11};
    const std::unordered_set<Location> CALLEE_SAVED = {Location::b, Location::r12, Location::r13, Location::r14, Location::r15};
    // FIXME : XMM LOCs not implemented

    // Return the indices of the inner values, or an empty optional if this combination doesn't match
    static std::optional<std::vector<size_t>> match_statement_combination(const ir::DependencyMatrix& graph, const arma::imat& subgraph, std::vector<size_t> statement_combination) {
        // Find which values are links between the selected statements (= which values have more than two edges in the statement combination)
        std::vector<size_t> link_columns;
        for (size_t column = 0; column < graph.values.size(); column++) {
            size_t nof_uses = 0;
            for (size_t row : statement_combination)
                if (graph.matrix(row, column) != 0)
                    nof_uses += 1;

            if (nof_uses >= 2)
                link_columns.push_back(column);
        }

        // Check all permutations for a match
        for (Permutations<size_t> column_permutations(link_columns, link_columns.size()); !column_permutations.done(); column_permutations.next()) {
            std::vector<size_t> columns = column_permutations.current();
            bool is_match = true;
            for (const auto& [subgraph_column, graph_column] : std::ranges::enumerate_view(columns)) {
                for (const auto& [subgraph_row, graph_row] : std::ranges::enumerate_view(statement_combination)) {
                    if (subgraph(subgraph_row, subgraph_column) != 0 && subgraph(subgraph_row, subgraph_column) != graph.matrix(graph_row, graph_column)) {
                        is_match = false;
                        goto exit_matching;
                    }
                }
            }
            exit_matching: ;

            if (is_match)
                return link_columns;
        }

        return {};
    }

    std::vector<GroupMatch> match_dependency_subgraph(TranslationGroupTag group, const ir::DependencyMatrix& graph, const std::vector<ir::StatementTag>& subgraph_tags, const arma::imat& subgraph) {
        if (graph.statements.size() < subgraph_tags.size())
            return {};

        std::vector<GroupMatch> matches;

        // Special case : no need for subgraph matching when the subgraph only has one node
        if (subgraph_tags.size() == 1) {
            for (std::shared_ptr<ir::DependencyNode> statement : graph.statements)
                if (statement->statement().tag == subgraph_tags[0])
                    matches.push_back(GroupMatch {.group = group, .statements = {statement}, .link_values = {}});
            return matches;
        }

        // General case : subgraph matching for subgraphs with more than one node
        std::vector<std::vector<size_t>> statement_matches(subgraph_tags.size());
        for (const auto& [node_index, node] : std::ranges::enumerate_view(graph.statements))
            for (const auto& [tag_index, tag] : std::ranges::enumerate_view(subgraph_tags))
                if (node->statement().tag == tag)
                    statement_matches[tag_index].push_back(node_index);

        for (const std::vector<size_t>& statement_match : statement_matches)
            if (statement_match.empty())
                return {};

        for (CartesianProduct<size_t> product(statement_matches); !product.done(); product.next()) {
            std::vector<size_t> combination = product.current();
            std::optional<std::vector<size_t>> link_columns = match_statement_combination(graph, subgraph, combination);
            if (link_columns.has_value()) {
                GroupMatch& match = matches.emplace_back(group);
                for (size_t statement_index : combination)
                    match.statements.push_back(graph.statements[statement_index]);
                for (size_t value_index : link_columns.value())
                    match.link_values.insert(graph.values[value_index]);
            }
        }

        return matches;
    }

    void update_translation_match(std::optional<TranslationMatch>& result, TranslationMatch&& match) {
        // Even a fixable non-match is better than nothing
        if (!result.has_value()) {
            result = match;
            return;
        }

        // FIXME : For now, keep the first match, since it's the first defined in the execmodel description
        if (result->matches())
            return;

        // At this point, `result` is a fixable non-match. If `match` is a match, replace
        if (match.matches()) {
            result = match;
            return;
        }

        const size_t result_transfers = *result->nof_transfers();
        const std::optional<size_t> match_transfers = match.nof_transfers();

        // If `match` is not fixable, skip
        if (!match_transfers.has_value())
            return;

        // Both are fixable non-matches : keep the one that needs the fewest transfers
        if (*match_transfers < result_transfers) {
            result = match;
            return;
        }
    }

    std::optional<Location> best_location(const std::unordered_set<Location> available_locations) {
        if (available_locations.empty())
            return {};

        Location best = *available_locations.begin();
        for (auto it = ++available_locations.begin(); it != available_locations.end(); it++)
            if (std::to_underlying(*it) < std::to_underlying(best))
                best = *it;

        return best;
    }
}
