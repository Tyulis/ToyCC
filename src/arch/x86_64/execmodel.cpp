#include "config.h"
#include "arch/x86_64/execmodel.h"
#include "util/strings.h"
#include "util/combinatorics.hpp"

namespace toycc::arch::x86_64 {
    const std::unordered_set<Location> CALLER_SAVED = {Location::a, Location::c, Location::d, Location::si, Location::di, Location::r8, Location::r9, Location::r10, Location::r11};
    const std::unordered_set<Location> CALLEE_SAVED = {Location::b, Location::r12, Location::r13, Location::r14, Location::r15};
    // FIXME : XMM LOCs not implemented

    std::string dump(const arma::imat& matrix) {
        std::stringstream stream;
        stream << matrix;
        return stream.str();
    }

    std::string dump(const arma::fmat& matrix) {
        std::stringstream stream;
        stream << matrix;
        return stream.str();
    }

    void print_mat(const arma::fmat& matrix) {
        matrix.print();
    }

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

        if (link_columns.size() < subgraph.n_cols)
            return {};

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

    StatementMatch select_statement_match(const std::vector<StatementMatch> matches) {
        if (matches[0].matches())
            return matches[0];

        StatementMatch selected_match = matches[0];
        for (auto it = matches.begin() + 1; it != matches.end(); it++) {
            const StatementMatch& current_match = *it;
            if (current_match.matches())
                return current_match;

            std::optional<size_t> current_nof_transfers = current_match.nof_transfers();
            std::optional<size_t> selected_nof_transfers = selected_match.nof_transfers();

            if (current_nof_transfers.has_value() && !selected_nof_transfers.has_value())
                selected_match = current_match;
            else if (current_nof_transfers.has_value() && selected_nof_transfers.has_value() && current_nof_transfers.value() < selected_nof_transfers.value())
                selected_match = current_match;
        }

        return selected_match;
    }

    void update_translation_match(std::optional<TranslationMatch>& result, TranslationMatch&& match) {
        if (toycc::config::debug::with_translation_trace)
            std::cerr << indent(dump(match), true, "        ") << "\n";

        if (!match.matches() && !match.nof_transfers().has_value())
            return;

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

    // -------- Dump functions

    std::ostream& operator<< (std::ostream& stream, const GroupMatch& match) {
        stream << match.group << " {";
        for (std::shared_ptr<ir::DependencyNode> node : match.statements)
            stream << node->statement().ir_code() << ", ";
        stream << "}";
        return stream;
    }

    std::ostream& operator<< (std::ostream& stream, const OperandMatch& match) {
        if (match.input_index.has_value())
            stream << match.input_index.value() << ":";

        switch (match.match) {
            case OperandMatch::OK:                 stream << "OK";  break;
            case OperandMatch::REQUIRES_TRANSFER:  stream << "REQUIRES_TRANSFER";  break;
            case OperandMatch::KO:                 stream << "KO";  break;
        }

        if (!match.locations.empty()) {
            stream << "(";
            for (const auto& [index, location] : std::ranges::enumerate_view(match.locations)) {
                if (index > 0)  stream << ", ";
                stream << location;
            }
            stream << ")";
        }
        return stream;
    }

    std::ostream& operator<< (std::ostream& stream, const TransferMatch& match) {
        stream << match.transfer;
        if (!match.source_locations.empty()) {
            stream << " from (";
            for (const auto& [index, location] : std::ranges::enumerate_view(match.source_locations)) {
                if (index > 0)  stream << ", ";
                stream << location;
            }
            stream << ")";
        }
        return stream;
    }

    std::ostream& operator<< (std::ostream& stream, const StatementMatch& match) {
        stream << "{";

        if (!match.input.empty()) {
            stream << "input: [";
            for (const auto& [index, operand] : std::ranges::enumerate_view(match.input)) {
                stream << operand;
                if (static_cast<size_t>(index) != match.input.size() - 1)
                    stream << ", ";
            }

            stream << "]";
            if (match.output.has_value())
                stream << ", ";
        }

        if (match.output.has_value())
            stream << "output: [" << match.output.value() << "]";

        stream << "}";
        return stream;
    }

    std::ostream& operator<< (std::ostream& stream, const TranslationMatch& match) {
        stream << match.translation << " {\n";
        stream << indent(dump(match.group_match), true, "    ") << "\n";
        for (const StatementMatch& statement : match.statements)
            stream << indent(dump(statement), true, "    ") << "\n";
        for (const OperandMatch& allocation : match.allocations)
            stream << allocation << ", ";
        stream << "}";
        return stream;
    }

    std::string dump(const GroupMatch& match) {
        std::stringstream stream;
        stream << match;
        return stream.str();
    }

    std::string dump(const OperandMatch& match) {
        std::stringstream stream;
        stream << match;
        return stream.str();
    }

    std::string dump(const TransferMatch& match) {
        std::stringstream stream;
        stream << match;
        return stream.str();
    }

    std::string dump(const StatementMatch& match) {
        std::stringstream stream;
        stream << match;
        return stream.str();
    }

    std::string dump(const TranslationMatch& match) {
        std::stringstream stream;
        stream << match;
        return stream.str();
    }
}
