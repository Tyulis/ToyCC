#pragma once

#include <string>
#include <memory>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>

#include "code_location.h"

namespace toycc::ir {
    using namespace boost::multi_index;

    enum class LabelType {
        NAMED, INTERNAL, FUNCTION,
    };

    struct Statement;
    struct Label {
        LabelType type;
        std::string name;
        std::shared_ptr<Statement> marker;
        CodeLocation location;
    };

    namespace LabelMapDetail {
        struct extract_label_name {
            using result_type = std::string;
            result_type operator() (std::shared_ptr<Label> label) const {
                return label->name;
            }
        };

        struct extract_label_marker {
            using result_type = std::shared_ptr<Statement>;
            result_type operator() (std::shared_ptr<Label> label) const {
                return label->marker;
            }
        };
    }

    struct name_index_tag {};
    struct marker_index_tag {};

    using LabelMap = multi_index_container<std::shared_ptr<Label>,
        indexed_by<hashed_unique    <tag<name_index_tag>,   LabelMapDetail::extract_label_name>,
                   hashed_non_unique<tag<marker_index_tag>, LabelMapDetail::extract_label_marker>>>;

    using LabelNameIndex   = LabelMap::index<name_index_tag  >::type;
    using LabelMarkerIndex = LabelMap::index<marker_index_tag>::type;
}

#include "ir/statement.h"
