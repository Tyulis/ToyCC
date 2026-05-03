#pragma once

#include <cstdint>

#include "output.h"
#include "debug/dwarf.h"

namespace toycc::debug {
    constexpr static inline size_t nof_significant_bits(size_t value) {
        if (value == 0)
            return 1;

        return 8*sizeof(value) - __builtin_clzll(value);
    }

    constexpr static inline size_t int_ceil_division(size_t value, size_t divisor) {
        return (value / divisor) + (value % divisor == 0? 0 : 1);
    }

    constexpr static inline size_t uleb128_size(size_t value) {
        return int_ceil_division(nof_significant_bits(value), 7);
    }

    constexpr static inline size_t sleb128_size(ssize_t value) {
        size_t absolute_value = llabs(value);
        return int_ceil_division(1 + nof_significant_bits(absolute_value), 7);
    }

    struct AssemblyData {
        std::string assembly;
        size_t length;

        bool operator== (const AssemblyData& other) const;
    };

    // More convenient encoder of DWARF debug information fields
    class Encoder {
        public:
            Encoder(DWARFFormat format, size_t size = 0);

            Encoder& int8(std::string expression);
            Encoder& int8(uint8_t value);
            Encoder& int8(int8_t value);
            Encoder& int8(Operation value);
            Encoder& int8(ChildDetermination value);
            Encoder& int8(CompilationUnitType value);
            Encoder& int8(LocationListEntryType value);

            Encoder& int16(std::string expression);
            Encoder& int16(uint16_t value);
            Encoder& int16(int16_t value);

            Encoder& int32(std::string expression);
            Encoder& int32(uint32_t value);
            Encoder& int32(int32_t value);

            Encoder& int64(std::string expression);
            Encoder& int64(uint64_t value);
            Encoder& int64(int64_t value);

            Encoder& address(std::string expression);  // Address on the target machine
            Encoder& address(uint64_t value);

            Encoder& offset(std::string expression);  // DWARF section offset (dependent on the DWARF format)
            Encoder& offset(size_t value);

            Encoder& uleb128(size_t value);
            Encoder& uleb128(std::string expression);
            Encoder& uleb128(Tag value);
            Encoder& uleb128(Attribute value);
            Encoder& uleb128(Form value);

            Encoder& sleb128(ssize_t value);
            Encoder& sleb128(std::string expression);

            Encoder& string(const std::string& value);  // NULL-terminated string

            Encoder& header(const CompilationUnitHeader& header);  // Emit a compilation unit header *except the length field*
            Encoder& header(const LocationListHeader& header);     // Emit a location lists section header *except the length field*

            Encoder& insert(const AssemblyData& data);
            Encoder& label(const std::string& name);

            size_t length() const;
            std::string str() const;

            AssemblyData encode() const;

        protected:
            CodeOutput assembly;
            DWARFFormat format;
            size_t size;

            static size_t length_field_length(DWARFFormat format);
    };

    // Specialization for the sections with an initial length field (.debug_info, .debug_loclists)
    class LengthFieldEncoder : public Encoder {
        public:
            LengthFieldEncoder(DWARFFormat format);
            std::string str() const;
    };

    CodeOutput& operator<< (CodeOutput& output, const Encoder& encoder);
    CodeOutput& operator<< (CodeOutput& output, const LengthFieldEncoder& encoder);
}

namespace std {
    template<> struct hash<toycc::debug::AssemblyData> {
        size_t operator() (const toycc::debug::AssemblyData& key) const {
            return std::hash<std::string>{}(key.assembly);
        }
    };
}
