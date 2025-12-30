#include "arch/x86_64/datamodel.h"

namespace toycc::arch::x86_64 {
    size_t DataModel::bool_size() const             { return  1; }
    size_t DataModel::bool_alignment() const        { return  1; }
    size_t DataModel::char_size() const             { return  1; }
    size_t DataModel::char_alignment() const        { return  1; }
    size_t DataModel::short_size() const            { return  2; }
    size_t DataModel::short_alignment() const       { return  2; }
    size_t DataModel::int_size() const              { return  4; }
    size_t DataModel::int_alignment() const         { return  4; }
    size_t DataModel::long_size() const             { return  8; }
    size_t DataModel::long_alignment() const        { return  8; }
    size_t DataModel::long_long_size() const        { return  8; }
    size_t DataModel::long_long_alignment() const   { return  8; }
    size_t DataModel::float_size() const            { return  4; }
    size_t DataModel::float_alignment() const       { return  4; }
    size_t DataModel::double_size() const           { return  8; }
    size_t DataModel::double_alignment() const      { return  8; }
    size_t DataModel::long_double_size() const      { return 16; }
    size_t DataModel::long_double_alignment() const { return 16; }
    size_t DataModel::pointer_size() const          { return  8; }
    size_t DataModel::pointer_alignment() const     { return  8; }

    DataModel DATAMODEL;
}
