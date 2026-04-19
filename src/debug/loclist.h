#pragma once

#include "debug/encoder.h"

namespace toycc::debug {
    class LocationList : public Encoder {
        public:
            LocationList& end();
    };

    Encoder& operator<< (Encoder& encoder, const LocationList& loclist);
}
