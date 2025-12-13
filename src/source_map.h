#pragma once

#include <string>
#include <vector>
#include <iostream>

namespace toycc {
    struct LinePosition {
        std::string filename;
        size_t line;
    };

    class LineMarker {
        public:
            LineMarker() = default;
            LineMarker(std::string marker);

            size_t line() const;
            std::string filename() const;
            bool begin() const;
            bool end() const;
            bool system() const;
            bool extern_c() const;

            LinePosition position(size_t nof_lines_since_marker = 0) const;

        private:
            size_t _line;
            std::string _filename;

            bool _begin;
            bool _end;
            bool _system;
            bool _extern_c;
    };

    class SourceMap {
        public:
            SourceMap() = default;
            SourceMap(std::istream& preprocessed_code, std::ostream& stripped_code);

            LinePosition at(unsigned stripped_line) const;
            void annotate(std::istream& stripped_code, std::ostream& annotated_code) const;

        private:
            std::vector<LinePosition> positions;
    };
}
