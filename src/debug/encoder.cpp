#include <format>
#include <utility>
#include "debug/encoder.h"
#include "debug/dwarf.h"

namespace toycc::debug {
    constexpr static size_t nof_significant_bits(size_t value) {
        if (value == 0)
            return 1;

        return 8*sizeof(value) - __builtin_clzll(value);
    }

    constexpr static size_t int_ceil_division(size_t value, size_t divisor) {
        return (value / divisor) + (value % divisor == 0? 0 : 1);
    }

    constexpr static size_t uleb128_size(size_t value) {
        return int_ceil_division(nof_significant_bits(value), 7);
    }

    // -------- Encoder
    Encoder::Encoder(size_t size) : size(size) {}

    Encoder& Encoder::int8(std::string expression) {
        assembly.directive(std::format(".byte {}", expression));
        size += 1;
        return *this;
    }
    Encoder& Encoder::int8(int8_t value)              {  return int8(std::to_string(static_cast<int>(value)));  }
    Encoder& Encoder::int8(uint8_t value)             {  return int8(std::to_string(static_cast<unsigned int>(value)));  }
    Encoder& Encoder::int8(ChildDetermination value)  {  return int8(std::to_underlying(value));  }
    Encoder& Encoder::int8(CompilationUnitType value) {  return int8(std::to_underlying(value));  }

    Encoder& Encoder::int16(std::string expression) {
        assembly.directive(std::format(".short {}", expression));
        size += 2;
        return *this;
    }
    Encoder& Encoder::int16(int16_t value)  {  return int16(std::to_string(value));  }
    Encoder& Encoder::int16(uint16_t value) {  return int16(std::to_string(value));  }

    Encoder& Encoder::int32(std::string expression) {
        assembly.directive(std::format(".int {}", expression));
        size += 4;
        return *this;
    }
    Encoder& Encoder::int32(int32_t value)  {  return int32(std::to_string(value));  }
    Encoder& Encoder::int32(uint32_t value) {  return int32(std::to_string(value));  }

    Encoder& Encoder::int64(std::string expression) {
        assembly.directive(std::format(".quad {}", expression));
        size += 8;
        return *this;
    }
    Encoder& Encoder::int64(int64_t value)  {  return int64(std::to_string(value));  }
    Encoder& Encoder::int64(uint64_t value) {  return int64(std::to_string(value));  }

    Encoder& Encoder::offset(std::string expression) {
        assembly.directive(std::format(".quad {}", expression));
        size += 8;
        return *this;
    }
    Encoder& Encoder::offset(uint64_t value) {  return offset(std::to_string(value));  }

    Encoder& Encoder::uleb128(size_t value) {
        assembly.directive(std::format(".uleb128 {}", value));
        size += uleb128_size(value);
        return *this;
    }
    Encoder& Encoder::uleb128(Tag value)       {  return uleb128(std::to_underlying(value));  }
    Encoder& Encoder::uleb128(Attribute value) {  return uleb128(std::to_underlying(value));  }
    Encoder& Encoder::uleb128(Form value)      {  return uleb128(std::to_underlying(value));  }

    size_t Encoder::length() const {
        return size;
    }

    std::string Encoder::str() const {
        return assembly.str();
    }

    // -------- DebugInfoEncoder
    DebugInfoEncoder::DebugInfoEncoder() : Encoder(12) {}  // Reserved for the length header (0xFFFFFFFF + 64-bits length)

    DebugInfoEncoder& DebugInfoEncoder::header(const CompilationUnitHeader& header) {
        int16 (header.version);
        int8  (header.unit_type);
        int8  (header.address_size);
        offset(header.debug_abbrev_label);
        return *this;
    }

    std::string DebugInfoEncoder::str() const {
        CodeOutput output;

        // Prepend the length header
        output.directive(".int 0xFFFFFFFF");  // Length field : set 64-bit DWARF
        output.directive(std::format(".quad {}", length() - 12));  // Length field excluded
        output << assembly.str();
        return output.str();
    }

    // -------- Stream operators
    CodeOutput& operator<< (CodeOutput& output, const Encoder& encoder) {
        output << encoder.str();
        return output;
    }

    CodeOutput& operator<< (CodeOutput& output, const DebugInfoEncoder& encoder) {
        output << encoder.str();
        return output;
    }
}
