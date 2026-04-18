#pragma once

#include <cstdint>

#include "output.h"
#include "debug/dwarf.h"

namespace toycc::debug {
    // More convenient encoder of DWARF debug information fields
    class Encoder {
        public:
            Encoder(size_t size = 0);

            Encoder& int8(std::string expression);
            Encoder& int8(uint8_t value);
            Encoder& int8(int8_t value);
            Encoder& int8(ChildDetermination value);
            Encoder& int8(CompilationUnitType value);

            Encoder& int16(std::string expression);
            Encoder& int16(uint16_t value);
            Encoder& int16(int16_t value);

            Encoder& int32(std::string expression);
            Encoder& int32(uint32_t value);
            Encoder& int32(int32_t value);

            Encoder& int64(std::string expression);
            Encoder& int64(uint64_t value);
            Encoder& int64(int64_t value);

            Encoder& offset(std::string expression);
            Encoder& offset(uint64_t value);

            Encoder& uleb128(size_t value);
            Encoder& uleb128(Tag value);
            Encoder& uleb128(Attribute value);
            Encoder& uleb128(Form value);

            size_t length() const;
            std::string str() const;

        protected:
            CodeOutput assembly;
            size_t size;
    };

    // Specialization for the .debug_info section
    class DebugInfoEncoder : public Encoder {
        public:
            DebugInfoEncoder();
            DebugInfoEncoder& header(const CompilationUnitHeader& header);  // Emit a compilation unit header *except the length field*

            std::string str() const;
    };

    CodeOutput& operator<< (CodeOutput& output, const Encoder& encoder);
    CodeOutput& operator<< (CodeOutput& output, const DebugInfoEncoder& encoder);
}
