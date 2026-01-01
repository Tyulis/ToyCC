#include <sstream>

#include "diagnostic.h"
#include "ir/flow.h"
#include "util/strings.h"

namespace toycc::ir {
    static std::string local_block_type_repr(LocalBlockType type) {
        switch (type) {
            case LocalBlockType::ENTRY:  return "ENTRY";
            case LocalBlockType::INNER:  return "INNER";
            case LocalBlockType::EXIT:   return "EXIT";
            default: throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unknown local block type");
        }
    }

    // -------- LocalBlock
    std::string LocalBlock::ir_code() const {
        std::stringstream code;
        code << local_block_type_repr(type) << " ";
        if (label.get() != nullptr)
            code << label->name;

        if (!statements.empty()) {
            code << " {\n";

            for (std::shared_ptr<Statement> statement : statements) {
                std::string statement_code = statement->ir_code();
                if (!statement_code.empty())
                    code << "    " << statement_code << ";\n";
            }

            code << "}";
        }
        return code.str();
    }

    // -------- Procedure
    Procedure::Procedure(std::shared_ptr<Declaration> declaration, LabelMap labels)
        : declaration(declaration), labels(labels), entry_block(blocks.emplace_node(LocalBlockType::ENTRY)), exit_block(blocks.emplace_node(LocalBlockType::EXIT)) {}

    std::shared_ptr<Label> Procedure::find_label(std::string name) const {
        auto& index = labels.get<name_index_tag>();
        auto element = index.find(name);
        if (element == index.end())  return nullptr;
        else                         return *element;
    }

    std::shared_ptr<Label> Procedure::find_label(std::shared_ptr<Statement> marker) const {
        auto& index = labels.get<marker_index_tag>();
        auto element = index.find(marker);
        if (element == index.end())  return nullptr;
        else                         return *element;
    }

    std::string Procedure::ir_code() const {
        std::stringstream code;
        code << "PROCEDURE " << declaration->name << " {\n";

        std::vector<std::shared_ptr<LocalBlock>> block_order;
        std::unordered_map<std::shared_ptr<LocalBlock>, size_t> block_indices;
        auto push_block = [&](std::shared_ptr<LocalBlock> block) {
            if (!block_indices.contains(block)) {
                block_indices[block] = block_order.size();
                block_order.push_back(block);
            }
        };

        for (std::shared_ptr<LocalBlock> source : blocks.sources())
            blocks.breadth_first_search(source, push_block);

        for (size_t index = 0; index < block_order.size(); index++) {
            std::shared_ptr<LocalBlock> block = block_order[index];
            code << "    " << index << " : " << indent(block->ir_code(), false, "    ");
            FlowGraph::EdgeSet out_edges = blocks.out_edges(block);

            if (out_edges.size() > 0) {
                code << " -> {";
                size_t exit_index = 0;
                for (FlowGraph::Edge edge : out_edges) {
                    code << block_indices[edge.exit];
                    if (exit_index != out_edges.size() - 1)
                        code << ", ";
                    exit_index += 1;
                }
                code << "}";
            }
            code << ";\n";
        }

        code << "}";
        return code.str();
    }

    // -------- TranslationUnit
    std::string TranslationUnit::ir_code() const {
        std::stringstream code;
        for (const auto& [name, declaration] : globals)
            code << declaration->ir_code() << ";\n";

        code << "\n";
        for (const auto& [name, procedure] : procedures)
            code << procedure.ir_code() << "\n\n";

        return code.str();
    }
}
