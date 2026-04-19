#include <format>
#include <utility>
#include "debug/encoder.h"
#include "debug/dwarf.h"

namespace toycc::debug {
    // -------- AssemblyData
    bool AssemblyData::operator== (const AssemblyData& other) const {
        return assembly == other.assembly;
    }

    // -------- Encoder
    Encoder::Encoder(DWARFFormat format, size_t size) : format(format), size(size) {}

    Encoder& Encoder::int8(std::string expression) {
        assembly.directive(std::format(".byte {}", expression));
        size += 1;
        return *this;
    }
    Encoder& Encoder::int8(int8_t value)                {  return int8(std::to_string(static_cast<int>(value)));  }
    Encoder& Encoder::int8(uint8_t value)               {  return int8(std::to_string(static_cast<unsigned int>(value)));  }
    Encoder& Encoder::int8(Operation value)             {  return int8(std::to_underlying(value));  }
    Encoder& Encoder::int8(ChildDetermination value)    {  return int8(std::to_underlying(value));  }
    Encoder& Encoder::int8(CompilationUnitType value)   {  return int8(std::to_underlying(value));  }
    Encoder& Encoder::int8(LocationListEntryType value) {  return int8(std::to_underlying(value));  }

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

    Encoder& Encoder::address(std::string expression) {
        assembly.directive(std::format(".quad {}", expression));
        size += 8;
        return *this;
    }
    Encoder& Encoder::address(uint64_t value) {  return address(std::to_string(value));  }

    Encoder& Encoder::offset(std::string expression) {
        switch (format) {
            case DWARFFormat::DWARF32:
                assembly.directive(std::format(".int {}", expression));
                size += 4;
                break;

            case DWARFFormat::DWARF64:
                assembly.directive(std::format(".quad {}", expression));
                size += 8;
                break;
        }
        return *this;
    }
    Encoder& Encoder::offset(size_t value) {  return offset(std::to_string(value));  }

    Encoder& Encoder::uleb128(size_t value) {
        assembly.directive(std::format(".uleb128 {}", value));
        size += uleb128_size(value);
        return *this;
    }
    Encoder& Encoder::uleb128(Tag value)       {  return uleb128(std::to_underlying(value));  }
    Encoder& Encoder::uleb128(Attribute value) {  return uleb128(std::to_underlying(value));  }
    Encoder& Encoder::uleb128(Form value)      {  return uleb128(std::to_underlying(value));  }

    Encoder& Encoder::sleb128(ssize_t value) {
        assembly.directive(std::format(".sleb128 {}", value));
        size += sleb128_size(value);
        return *this;
    }

    Encoder& Encoder::header(const CompilationUnitHeader& header) {
        int16 (header.version);
        int8  (header.unit_type);
        int8  (header.address_size);
        offset(header.debug_abbrev_label);
        return *this;
    }

    Encoder& Encoder::header(const LocationListHeader& header) {
        int16(header.version);
        int8 (header.address_size);
        int8 (header.segment_selector_size);
        int32(header.offset_entry_count);
        return *this;
    }

    Encoder& Encoder::insert(const AssemblyData& data) {
        assembly << data.assembly;
        size += data.length;
        return *this;
    }

    size_t Encoder::length() const {
        return size;
    }

    std::string Encoder::str() const {
        return assembly.str();
    }

    AssemblyData Encoder::encode() const {
        return {.assembly = str(), .length = length()};
    }

    size_t Encoder::length_field_length(DWARFFormat format) {
        switch (format) {
            case DWARFFormat::DWARF32: return 4;
            case DWARFFormat::DWARF64: return 12;
        }
        __builtin_unreachable();
    }


    // -------- LengthFieldEncoder
    LengthFieldEncoder::LengthFieldEncoder(DWARFFormat format) : Encoder(format, length_field_length(format)) {}  // Reserved for the length header

    std::string LengthFieldEncoder::str() const {
        CodeOutput output;

        // Prepend the length field
        const size_t content_length = length() - length_field_length(format);
        switch (format) {
            case DWARFFormat::DWARF32:
                output.directive(std::format(".int {}", content_length));
                break;

            case DWARFFormat::DWARF64:
                output.directive(".int 0xFFFFFFFF");  // Length field : set 64-bit DWARF
                output.directive(std::format(".quad {}", content_length));  // Length field excluded
                break;
        }

        output << assembly.str();
        return output.str();
    }


    // -------- Stream operators
    CodeOutput& operator<< (CodeOutput& output, const Encoder& encoder) {
        output << encoder.str();
        return output;
    }

    CodeOutput& operator<< (CodeOutput& output, const LengthFieldEncoder& encoder) {
        output << encoder.str();
        return output;
    }
}
