#include <limits>

#include "debug/dwarf.h"
#include "debug/expression.h"

namespace toycc::debug {
    Expression::Expression(DWARFFormat format) : Encoder(format) {}

    Expression& Expression::reg_location(size_t index) {
        auto it = OP_REGISTER_LOCATION.find(index);
        if (it == OP_REGISTER_LOCATION.end()) {
            int8(Operation::DW_OP_regx);
            uleb128(index);
        } else {
            int8(it->second);
        }
        return *this;
    }

    Expression& Expression::reg_value(size_t index, ssize_t offset) {
        auto it = OP_REGISTER_VALUE.find(index);
        if (it == OP_REGISTER_VALUE.end()) {
            int8(Operation::DW_OP_bregx);
            uleb128(index);
        } else {
            int8(it->second);
        }
        sleb128(offset);
        return *this;
    }

    Expression& Expression::address(uint64_t value) {
        return address(std::to_string(value));
    }

    Expression& Expression::address(std::string label) {
        int8(Operation::DW_OP_addr);
        Encoder::address(label);
        return *this;
    }

    Expression& Expression::signed_constant(ssize_t value) {
        if (value >= 0) {
            auto it = OP_LITERAL_VALUE.find(static_cast<size_t>(value));
            if (it != OP_LITERAL_VALUE.end()) {
                int8(it->second);
                return *this;
            }
        }

        if (std::numeric_limits<int8_t>::min() <= value && value <= std::numeric_limits<int8_t>::max()) {
            int8(Operation::DW_OP_const1s);
            int8(static_cast<int8_t>(value));
        } else if (std::numeric_limits<int16_t>::min() <= value && value <= std::numeric_limits<int16_t>::max()) {
            int8(Operation::DW_OP_const2s);
            int16(static_cast<int16_t>(value));
        } else if (std::numeric_limits<int32_t>::min() <= value && value <= std::numeric_limits<int32_t>::max()) {
            int8(Operation::DW_OP_const4s);
            int32(static_cast<int32_t>(value));
        } else if (std::numeric_limits<int64_t>::min() <= value && value <= std::numeric_limits<int64_t>::max()) {
            int8(Operation::DW_OP_const8s);
            int64(static_cast<int64_t>(value));
        } else {
            int8(Operation::DW_OP_consts);
            sleb128(value);
        }
        return *this;
    }

    Expression& Expression::unsigned_constant(size_t value) {
        auto it = OP_LITERAL_VALUE.find(value);
        if (it != OP_LITERAL_VALUE.end()) {
            int8(it->second);
            return *this;
        }

        if (std::numeric_limits<uint8_t>::min() <= value && value <= std::numeric_limits<uint8_t>::max()) {
            int8(Operation::DW_OP_const1u);
            int8(static_cast<uint8_t>(value));
        } else if (std::numeric_limits<uint16_t>::min() <= value && value <= std::numeric_limits<uint16_t>::max()) {
            int8(Operation::DW_OP_const2u);
            int16(static_cast<uint16_t>(value));
        } else if (std::numeric_limits<uint32_t>::min() <= value && value <= std::numeric_limits<uint32_t>::max()) {
            int8(Operation::DW_OP_const4u);
            int32(static_cast<uint32_t>(value));
        } else if (std::numeric_limits<uint64_t>::min() <= value && value <= std::numeric_limits<uint64_t>::max()) {
            int8(Operation::DW_OP_const8u);
            int64(static_cast<uint64_t>(value));
        } else {
            int8(Operation::DW_OP_constu);
            sleb128(value);
        }
        return *this;
    }

    Expression& Expression::stack_offset(ssize_t offset) {
        int8(Operation::DW_OP_fbreg);
        sleb128(offset);
        return *this;
    }

    Expression& Expression::call_frame_cfa() {
        int8(Operation::DW_OP_call_frame_cfa);
        return *this;
    }

    size_t Expression::length() const {
        return uleb128_size(size) + size;
    }

    std::string Expression::str() const {
        // Add the length field
        Encoder encoder(format);
        encoder.uleb128(size);
        encoder.insert({.assembly = assembly.str(), .length = size});
        return encoder.str();
    }

    AssemblyData Expression::encode() const {
        return AssemblyData {.assembly = str(), .length = length()};
    }
}
