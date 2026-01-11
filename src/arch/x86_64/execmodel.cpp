#include <utility>
#include "arch/x86_64/execmodel.h"

namespace toycc::arch::x86_64 {
    const std::unordered_set<Location> CALLER_SAVED = {Location::a, Location::c, Location::d, Location::si, Location::di, Location::r8, Location::r9, Location::r10, Location::r11};
    const std::unordered_set<Location> CALLEE_SAVED = {Location::b, Location::r12, Location::r13, Location::r14, Location::r15};
    // FIXME : XMM LOCs not implemented

    const std::unordered_map<Location, std::unordered_map<size_t, std::string>> REGISTER_NAMES= {
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

    std::unordered_set<std::shared_ptr<ir::DependencyNode>> get_entry_statements(const ir::DependencyGraph& graph) {
        std::unordered_set<std::shared_ptr<ir::DependencyNode>> entry_statements;
        for (std::shared_ptr<ir::DependencyNode> entry_node : graph.sources()) {
            if (entry_node->is_statement())
                entry_statements.insert(entry_node);
            else
                for (std::shared_ptr<ir::DependencyNode> next_node : graph.next_nodes(entry_node))
                    entry_statements.insert(next_node);
        }
        return entry_statements;
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
