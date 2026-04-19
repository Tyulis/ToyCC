#pragma once

#include "debug/encoder.h"

namespace toycc::debug {
    class Location : public Encoder {
        public:
            Location& reg(Operation operation);
        private:
    };

    class LocationList : public Encoder {
        public:
            LocationList& end();
    };

    Encoder& operator<< (Encoder& encoder, const LocationList& loclist);
}
