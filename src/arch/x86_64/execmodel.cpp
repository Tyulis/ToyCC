#include <utility>
#include "arch/x86_64/execmodel.h"
#include "util/combinatorics.hpp"

namespace toycc::arch::x86_64 {
    const std::unordered_set<Location> CALLER_SAVED = {Location::a, Location::c, Location::d, Location::si, Location::di, Location::r8, Location::r9, Location::r10, Location::r11};
    const std::unordered_set<Location> CALLEE_SAVED = {Location::b, Location::r12, Location::r13, Location::r14, Location::r15};
    // FIXME : XMM LOCs not implemented

    const std::unordered_map<Location, std::unordered_map<size_t, std::string>> REGISTER_NAMES = {
        {Location::a,    {{1,   "%al"}, {2,   "%ax"}, {4,  "%eax"}, {8, "%rax"}}},
        {Location::b,    {{1,   "%bl"}, {2,   "%bx"}, {4,  "%ebx"}, {8, "%rbx"}}},
        {Location::c,    {{1,   "%cl"}, {2,   "%cx"}, {4,  "%ecx"}, {8, "%rcx"}}},
        {Location::d,    {{1,   "%dl"}, {2,   "%dx"}, {4,  "%edx"}, {8, "%rdx"}}},
        {Location::si,   {{1,  "%sil"}, {2,   "%si"}, {4,  "%esi"}, {8, "%rsi"}}},
        {Location::di,   {{1,  "%dil"}, {2,   "%di"}, {4,  "%edi"}, {8, "%rdi"}}},
        {Location::sp,   {{1,  "%spl"}, {2,   "%sp"}, {4,  "%esp"}, {8, "%rsp"}}},
        {Location::bp,   {{1,  "%bpl"}, {2,   "%bp"}, {4,  "%ebp"}, {8, "%rbp"}}},
        {Location::r8,   {{1,  "%r8b"}, {2,  "%r8w"}, {4,  "%r8d"}, {8,  "%r8"}}},
        {Location::r9,   {{1,  "%r9b"}, {2,  "%r9w"}, {4,  "%r9d"}, {8,  "%r9"}}},
        {Location::r10,  {{1, "%r10b"}, {2, "%r10w"}, {4, "%r10d"}, {8, "%r10"}}},
        {Location::r11,  {{1, "%r11b"}, {2, "%r11w"}, {4, "%r11d"}, {8, "%r11"}}},
        {Location::r12,  {{1, "%r12b"}, {2, "%r12w"}, {4, "%r12d"}, {8, "%r12"}}},
        {Location::r13,  {{1, "%r13b"}, {2, "%r13w"}, {4, "%r13d"}, {8, "%r13"}}},
        {Location::r14,  {{1, "%r14b"}, {2, "%r14w"}, {4, "%r14d"}, {8, "%r14"}}},
        {Location::r15,  {{1, "%r15b"}, {2, "%r15w"}, {4, "%r15d"}, {8, "%r15"}}},
    };

    static bool match_statement_combination(const ir::DependencyMatrix& graph, const arma::imat& subgraph, std::vector<size_t> statement_combination) {
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
                return true;
        }

        return false;
    }

    std::vector<std::vector<std::shared_ptr<ir::DependencyNode>>> match_dependency_subgraph
            (const ir::DependencyMatrix& graph, const std::vector<ir::StatementTag> subgraph_tags, const arma::imat& subgraph)
    {
        if (graph.statements.size() < subgraph_tags.size())
            return {};

        std::vector<std::vector<std::shared_ptr<ir::DependencyNode>>> matches;

        // Special case : no need for subgraph matching when the subgraph only has one node
        if (subgraph_tags.size() == 1) {
            for (std::shared_ptr<ir::DependencyNode> statement : graph.statements) {
                if (statement->statement().tag == subgraph_tags[0]) {
                    matches.emplace_back(1);
                    matches.back()[0] = statement;
                }
            }
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
            if (match_statement_combination(graph, subgraph, combination)) {
                matches.emplace_back(combination.size());
                for (const auto& [set_index, statement_index] : std::ranges::enumerate_view(combination))
                    matches.back()[set_index] = graph.statements[statement_index];
            }
        }

        return matches;
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
