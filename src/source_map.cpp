#include "source_map.h"
#include "diagnostic.h"
#include "util/strings.h"

namespace toycc {
    // -------- LineMarker
    LineMarker::LineMarker(std::string marker) : _begin(false), _end(false), _system(false), _extern_c(false) {
        std::vector<std::string> tokens = split_one_of(marker);

        if (tokens[0] != "#")
            throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, "Invalid line marker format : the line should start with a `#`");

        _line = std::stoi(tokens[1]);
        _filename = trim(tokens[2], "\"");

        for (auto it = tokens.begin() + 3; it != tokens.end(); it++) {
            switch ((*it)[0]) {
                case '1':  _begin    = true;  break;
                case '2':  _end      = true;  break;
                case '3':  _system   = true;  break;
                case '4':  _extern_c = true;  break;
                default:
                    throw Diagnostic(Diagnostic::Level::ERROR, std::format("Unknown flag {} in the line marker", *it));
            }
        }
    }

    size_t      LineMarker::line()     const { return _line;     }
    std::string LineMarker::filename() const { return _filename; }
    bool        LineMarker::begin()    const { return _begin;    }
    bool        LineMarker::end()      const { return _end;      }
    bool        LineMarker::system()   const { return _system;   }
    bool        LineMarker::extern_c() const { return _extern_c; }

    LinePosition LineMarker::position(size_t nof_lines_since_marker) const {
        return {.filename = _filename, .line = _line + nof_lines_since_marker};
    }


    // -------- IncludeStack
    SourceMap::SourceMap(std::istream& code, std::ostream& stripped_code) {
        std::string preprocessed_line;
        size_t preprocessed_line_number = 1;
        size_t preprocessed_lines_since_last_marker = 0;

        LineMarker last_marker;


        while (!code.eof()) {
            std::getline(code, preprocessed_line);
            rtrim_inplace(preprocessed_line);
            std::string stripped_line = trim(preprocessed_line);

            if (stripped_line.empty()) {
                preprocessed_lines_since_last_marker += 1;  // Skip
            } else if (stripped_line[0] == '#' && !stripped_line.substr(stripped_line.find_first_not_of("#" + WHITESPACE)).starts_with("pragma")) {
                try {
                    last_marker = LineMarker(stripped_line);
                    preprocessed_lines_since_last_marker = 0;
                } catch (Diagnostic& diagnostic) {
                    diagnostic.line(preprocessed_line_number);
                    throw diagnostic;
                }
            } else {
                positions.push_back(last_marker.position(preprocessed_lines_since_last_marker));
                stripped_code << preprocessed_line << "\n";  // Keep the leading whitespace in the parsed line, so the character positions stay consistent with the source code
                preprocessed_lines_since_last_marker += 1;
            }
            preprocessed_line_number += 1;
        }
    }

    LinePosition SourceMap::at(unsigned stripped_line) const {
        if (stripped_line == 0 || stripped_line >= positions.size())
            throw Diagnostic(Diagnostic::Level::INTERNAL_ERROR, std::format("Attempted to access non-existing stripped line {}/{}", stripped_line, positions.size()));
        return positions.at(stripped_line - 1);
    }

    void SourceMap::annotate(std::istream& stripped_code, std::ostream& annotated_code) const {
        std::string line;

        for (LinePosition position : positions) {
            std::getline(stripped_code, line);
            line += std::format("  // {}:{}", position.filename, position.line);
            annotated_code << line << "\n";
        }
    }
}
