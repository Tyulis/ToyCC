#include "debug/dwarf.h"

namespace toycc::debug {
    std::ostream& operator<< (std::ostream& stream, DWARFFormat format) {
        switch (format) {
            case DWARFFormat::DWARF32:  return stream << "DWARF32";
            case DWARFFormat::DWARF64:  return stream << "DWARF64";
        }
        __builtin_unreachable();
    }

    const std::unordered_map<Form, Class> TO_CLASS = {
        {Form::DW_FORM_addr,           Class::address},
        {Form::DW_FORM_block2,         Class::block},
        {Form::DW_FORM_block4,         Class::block},
        {Form::DW_FORM_data2,          Class::constant},
        {Form::DW_FORM_data4,          Class::constant},
        {Form::DW_FORM_data8,          Class::constant},
        {Form::DW_FORM_string,         Class::string},
        {Form::DW_FORM_block,          Class::block},
        {Form::DW_FORM_block1,         Class::block},
        {Form::DW_FORM_data1,          Class::constant},
        {Form::DW_FORM_flag,           Class::flag},
        {Form::DW_FORM_sdata,          Class::constant},
        {Form::DW_FORM_strp,           Class::string},
        {Form::DW_FORM_udata,          Class::constant},
        {Form::DW_FORM_ref_addr,       Class::reference},
        {Form::DW_FORM_ref1,           Class::reference},
        {Form::DW_FORM_ref2,           Class::reference},
        {Form::DW_FORM_ref4,           Class::reference},
        {Form::DW_FORM_ref8,           Class::reference},
        {Form::DW_FORM_ref_udata,      Class::reference},
        {Form::DW_FORM_indirect,       Class::indirect},
        {Form::DW_FORM_sec_offset,     Class::pointer},
        {Form::DW_FORM_exprloc,        Class::exprloc},
        {Form::DW_FORM_flag_present,   Class::flag},
        {Form::DW_FORM_strx,           Class::string},
        {Form::DW_FORM_addrx,          Class::address},
        {Form::DW_FORM_ref_sup4,       Class::reference},
        {Form::DW_FORM_strp_sup,       Class::string},
        {Form::DW_FORM_data16,         Class::constant},
        {Form::DW_FORM_line_strp,      Class::string},
        {Form::DW_FORM_ref_sig8,       Class::reference},
        {Form::DW_FORM_implicit_const, Class::constant},
        {Form::DW_FORM_loclistx,       Class::loclist},
        {Form::DW_FORM_rnglistx,       Class::rnglist},
        {Form::DW_FORM_ref_sup8,       Class::reference},
        {Form::DW_FORM_strx1,          Class::string},
        {Form::DW_FORM_strx2,          Class::string},
        {Form::DW_FORM_strx3,          Class::string},
        {Form::DW_FORM_strx4,          Class::string},
        {Form::DW_FORM_addrx1,         Class::address},
        {Form::DW_FORM_addrx2,         Class::address},
        {Form::DW_FORM_addrx3,         Class::address},
        {Form::DW_FORM_addrx4,         Class::address},
    };

    const std::unordered_map<size_t, Operation> OP_REGISTER_LOCATION = {
        { 0, Operation::DW_OP_reg0},  { 1, Operation::DW_OP_reg1},  { 2, Operation::DW_OP_reg2},  { 3, Operation::DW_OP_reg3},
        { 4, Operation::DW_OP_reg4},  { 5, Operation::DW_OP_reg5},  { 6, Operation::DW_OP_reg6},  { 7, Operation::DW_OP_reg7},
        { 8, Operation::DW_OP_reg8},  { 9, Operation::DW_OP_reg9},  {10, Operation::DW_OP_reg10}, {11, Operation::DW_OP_reg11},
        {12, Operation::DW_OP_reg12}, {13, Operation::DW_OP_reg13}, {14, Operation::DW_OP_reg14}, {15, Operation::DW_OP_reg15},
        {16, Operation::DW_OP_reg16}, {17, Operation::DW_OP_reg17}, {18, Operation::DW_OP_reg18}, {19, Operation::DW_OP_reg19},
        {20, Operation::DW_OP_reg20}, {21, Operation::DW_OP_reg21}, {22, Operation::DW_OP_reg22}, {23, Operation::DW_OP_reg23},
        {24, Operation::DW_OP_reg24}, {25, Operation::DW_OP_reg25}, {26, Operation::DW_OP_reg26}, {27, Operation::DW_OP_reg27},
        {28, Operation::DW_OP_reg28}, {29, Operation::DW_OP_reg29}, {30, Operation::DW_OP_reg30}, {31, Operation::DW_OP_reg31},
    };

    const std::unordered_map<size_t, Operation> OP_REGISTER_VALUE = {
        { 0, Operation::DW_OP_breg0},  { 1, Operation::DW_OP_breg1},  { 2, Operation::DW_OP_breg2},  { 3, Operation::DW_OP_breg3},
        { 4, Operation::DW_OP_breg4},  { 5, Operation::DW_OP_breg5},  { 6, Operation::DW_OP_breg6},  { 7, Operation::DW_OP_breg7},
        { 8, Operation::DW_OP_breg8},  { 9, Operation::DW_OP_breg9},  {10, Operation::DW_OP_breg10}, {11, Operation::DW_OP_breg11},
        {12, Operation::DW_OP_breg12}, {13, Operation::DW_OP_breg13}, {14, Operation::DW_OP_breg14}, {15, Operation::DW_OP_breg15},
        {16, Operation::DW_OP_breg16}, {17, Operation::DW_OP_breg17}, {18, Operation::DW_OP_breg18}, {19, Operation::DW_OP_breg19},
        {20, Operation::DW_OP_breg20}, {21, Operation::DW_OP_breg21}, {22, Operation::DW_OP_breg22}, {23, Operation::DW_OP_breg23},
        {24, Operation::DW_OP_breg24}, {25, Operation::DW_OP_breg25}, {26, Operation::DW_OP_breg26}, {27, Operation::DW_OP_breg27},
        {28, Operation::DW_OP_breg28}, {29, Operation::DW_OP_breg29}, {30, Operation::DW_OP_breg30}, {31, Operation::DW_OP_breg31},
    };

    const std::unordered_map<size_t, Operation> OP_LITERAL_VALUE{
        { 0, Operation::DW_OP_lit0},  { 1, Operation::DW_OP_lit1},  { 2, Operation::DW_OP_lit2},  { 3, Operation::DW_OP_lit3},
        { 4, Operation::DW_OP_lit4},  { 5, Operation::DW_OP_lit5},  { 6, Operation::DW_OP_lit6},  { 7, Operation::DW_OP_lit7},
        { 8, Operation::DW_OP_lit8},  { 9, Operation::DW_OP_lit9},  {10, Operation::DW_OP_lit10}, {11, Operation::DW_OP_lit11},
        {12, Operation::DW_OP_lit12}, {13, Operation::DW_OP_lit13}, {14, Operation::DW_OP_lit14}, {15, Operation::DW_OP_lit15},
        {16, Operation::DW_OP_lit16}, {17, Operation::DW_OP_lit17}, {18, Operation::DW_OP_lit18}, {19, Operation::DW_OP_lit19},
        {20, Operation::DW_OP_lit20}, {21, Operation::DW_OP_lit21}, {22, Operation::DW_OP_lit22}, {23, Operation::DW_OP_lit23},
        {24, Operation::DW_OP_lit24}, {25, Operation::DW_OP_lit25}, {26, Operation::DW_OP_lit26}, {27, Operation::DW_OP_lit27},
        {28, Operation::DW_OP_lit28}, {29, Operation::DW_OP_lit29}, {30, Operation::DW_OP_lit30}, {31, Operation::DW_OP_lit31},
    };
}
