#pragma once

#include "debug/encoder.h"

namespace toycc::debug {
    class Expression : public Encoder {
        public:
            Expression& reg_location(size_t index);
            Expression& reg_value(size_t index, ssize_t offset = 0);
            Expression& address(uint64_t value);
            Expression& signed_constant(ssize_t value);
            Expression& unsigned_constant(size_t value);
            Expression& stack_offset(ssize_t offset);
            Expression& call_frame_cfa();

            size_t length() const;
            std::string str() const;
    };
}
