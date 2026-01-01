#pragma once

#include <memory>
#include <vector>

#include "ir/label.h"
#include "ir/statement.h"
#include "ir/declaration.h"

#include "util/graph.hpp"

namespace toycc::ir {
    enum class LocalBlockType {
        ENTRY, INNER, EXIT,
    };

    struct LocalBlock {
        LocalBlockType type;
        std::shared_ptr<Label> label;
        std::vector<std::shared_ptr<Statement>> statements;
        std::vector<std::shared_ptr<Declaration>> input_variables;
        std::vector<std::shared_ptr<Declaration>> output_variables;

        std::string ir_code() const;
    };

    using FlowGraph = Graph<LocalBlock>;

    struct Procedure {
        std::shared_ptr<Declaration> declaration;
        LabelMap labels;
        FlowGraph blocks;
        std::shared_ptr<LocalBlock> entry_block;
        std::shared_ptr<LocalBlock> exit_block;

        Procedure() = default;
        Procedure(std::shared_ptr<Declaration> declaration, LabelMap labels);

        std::shared_ptr<Label> find_label(std::string name) const;
        std::shared_ptr<Label> find_label(std::shared_ptr<Statement> marker) const;

        std::string ir_code() const;
    };

    struct TranslationUnit {
        std::unordered_map<std::string, std::shared_ptr<Declaration>> globals;
        std::unordered_map<std::string, Procedure> procedures;

        std::string ir_code() const;
    };
}
