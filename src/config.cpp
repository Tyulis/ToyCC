#include <sstream>

#include "config.h"
#include "debug/dwarf.h"
#include "util/strings.h"

namespace toycc::config {
    namespace debug {
        bool enable = false;
        bool with_default_location = false;
        toycc::debug::DWARFFormat format = toycc::debug::DWARFFormat::DWARF32;

        std::string dump() {
            std::stringstream output;
            output << std::boolalpha;
            output << "- enable : " << enable << "\n";
            output << "- with_default_location : " << with_default_location << "\n";
            output << "- format : " << format << "\n";
            return output.str();
        }
    }

    namespace optimization {
        bool split_intermediates = false;
        bool constant_folding = false;

        void set_level(size_t level) {
            // Everything starts at false, work our way down the levels
            switch (level) {
                default:
                case 1:
                    split_intermediates = true;
                    constant_folding = true;
                    [[fallthrough]];

                case 0:
                    break;
            }
        }

        std::string dump() {
            std::stringstream output;
            output << std::boolalpha;
            output << "- split_intermediates : " << split_intermediates << "\n";
            output << "- constant_folding : " << constant_folding << "\n";
            return output.str();
        }
    }

    namespace dev {
        bool with_comment_trace = false;
        bool with_location_trace = false;
        bool with_translation_trace = false;

        std::string dump() {
            std::stringstream output;
            output << std::boolalpha;
            output << "- with_comment_trace : " << with_comment_trace << "\n";
            output << "- with_location_trace : " << with_location_trace << "\n";
            output << "- with_translation_trace : " << with_translation_trace << "\n";
            return output.str();
        }
    }

    std::string dump() {
        std::stringstream output;
        output << "- debug\n";
        output << indent(debug::dump());
        output << "- optimization\n";
        output << indent(optimization::dump());
        output << "- dev\n";
        output << indent(dev::dump());
        return output.str();
    }
}
