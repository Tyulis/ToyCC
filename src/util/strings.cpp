#include <cctype>
#include <format>
#include <sstream>

#include "util/strings.h"

namespace toycc {
    std::vector<std::string> split(const std::string& str, const std::string& delimiter) {
        std::vector<std::string> tokens;
        size_t start = 0, pos = 0;
        while ((pos = str.find(delimiter, start)) != std::string::npos) {
            tokens.push_back(str.substr(start, pos - start));
            start = pos + delimiter.length();
        }
        tokens.push_back(str.substr(start));

        return tokens;
    }

    std::vector<std::string> split_one_of(const std::string& str, const std::string& delimiters) {
        std::vector<std::string> tokens;
        size_t start = 0, pos = 0;
        while ((pos = str.find_first_of(delimiters, start)) != std::string::npos) {
            tokens.push_back(str.substr(start, pos - start));
            start = pos + 1;
        }
        tokens.push_back(str.substr(start));

        return tokens;
    }

    std::string& ltrim_inplace(std::string& str, std::string characters) {
        str.erase(0, str.find_first_not_of(characters));
        return str;
    }

    std::string& rtrim_inplace(std::string& str, std::string characters) {
        size_t trimmed_end = str.find_last_not_of(characters);
        if (trimmed_end != std::string::npos)
            str.erase(trimmed_end + 1);
        return str;
    }

    std::string& trim_inplace(std::string& str, std::string characters) {
        ltrim_inplace(str, characters);
        rtrim_inplace(str, characters);
        return str;
    }

    std::string ltrim(const std::string& str, std::string characters) {
        std::string result = str;
        ltrim_inplace(result, characters);
        return result;
    }

    std::string rtrim(const std::string& str, std::string characters) {
        std::string result = str;
        rtrim_inplace(result, characters);
        return result;
    }

    std::string trim(const std::string& str, std::string characters) {
        std::string result = str;
        trim_inplace(result, characters);
        return result;
    }

    std::string& replace_inplace(std::string& str, std::string origin, std::string replacement) {
        size_t position = 0;
        while ((position = str.find(origin, position + replacement.length())) != std::string::npos)
            str.replace(position, origin.length(), replacement);

        return str;
    }

    std::string replace(const std::string& str, std::string origin, std::string replacement) {
        std::string result = str;
        replace_inplace(result, origin, replacement);
        return result;
    }

    std::string& to_printable_inplace(std::string& str) {
        size_t position = 0;
        while (position < str.length()) {
            if (std::isprint(str[position])) {
                position += 1;
                continue;
            }

            switch (str[position]) {
                case '\t':  str.replace(position, position + 1, "\\t");  position += 2;  break;
                case '\n':  str.replace(position, position + 1, "\\n");  position += 2;  break;
                case '\v':  str.replace(position, position + 1, "\\v");  position += 2;  break;
                case '\f':  str.replace(position, position + 1, "\\f");  position += 2;  break;
                case '\r':  str.replace(position, position + 1, "\\r");  position += 2;  break;
                default:
                    std::string replacement = std::format("\\x{:02X}", static_cast<unsigned int>(str[position]));
                    str.replace(position, position + 1, replacement);
                    position += replacement.length();
            }
        }

        return str;
    }

    std::string to_printable(const std::string& str) {
        std::string result = str;
        to_printable_inplace(result);
        return result;
    }

    std::string& escape_inplace(std::string& str) {
        to_printable_inplace(str);

        size_t position = 0;
        while (position < str.length()) {
            switch (str[position]) {
                case '"':  str.replace(position, position + 1, "\\\"");  position += 2;  break;
                default:                                                 position += 1;  break;
            }
        }

        return str;
    }

    std::string escape(const std::string& str) {
        std::string result = str;
        escape_inplace(result);
        return result;
    }

    std::string& xml_escape_inplace(std::string& str) {
        to_printable_inplace(str);
        size_t position = 0;
        while (position < str.length()) {
            switch (str[position]) {
                case '"':  str.replace(position, position + 1, "&quot;");  position += 6;  break;
                case '&':  str.replace(position, position + 1, "&amp;");   position += 5;  break;
                case '\'': str.replace(position, position + 1, "&apos;");  position += 6;  break;
                case '<':  str.replace(position, position + 1, "&lt;");    position += 4;  break;
                case '>':  str.replace(position, position + 1, "&gt;");    position += 4;  break;
                default:                                                   position += 1;  break;
            }
        }

        return str;
    }

    std::string xml_escape(const std::string& str) {
        std::string result = str;
        xml_escape_inplace(result);
        return result;
    }

    std::string indent(const std::string& str, bool indent_first_line, std::string prefix) {
        std::stringstream indented;
        std::vector<std::string> lines = split(str, "\n");
        for (unsigned line_index = 0; line_index < lines.size(); line_index++) {
            if (indent_first_line || line_index > 0)
                indented << prefix;
            indented << lines[line_index];
            if (line_index < lines.size() - 1)
                indented << "\n";
        }

        return indented.str();
    }
}
