#pragma once

#include <string>
#include <vector>

namespace toycc {
    constexpr std::string WHITESPACE = " \r\n\t\v";

    std::vector<std::string> split(const std::string& str, const std::string& delimiter);
    std::vector<std::string> split_one_of(const std::string& str, const std::string& delimiters = WHITESPACE);

    std::string& ltrim_inplace(std::string& str, std::string characters = WHITESPACE);
    std::string& rtrim_inplace(std::string& str, std::string characters = WHITESPACE);
    std::string& trim_inplace (std::string& str, std::string characters = WHITESPACE);

    [[nodiscard]] std::string ltrim(const std::string& str, std::string characters = WHITESPACE);
    [[nodiscard]] std::string rtrim(const std::string& str, std::string characters = WHITESPACE);
    [[nodiscard]] std::string trim (const std::string& str, std::string characters = WHITESPACE);

    std::string& replace_inplace(std::string& str, std::string origin, std::string replacement);
    std::string replace(const std::string& str, std::string origin, std::string replacement);

    std::string& to_printable_inplace(std::string& str);
    std::string to_printable(const std::string& str);

    std::string& escape_inplace(std::string& str);
    std::string escape(const std::string& str);

    std::string& xml_escape_inplace(std::string& str);
    std::string xml_escape(const std::string& str);

    std::string indent(const std::string& str, bool indent_first_line=true, std::string prefix="\t");

    std::string& to_lower_inplace(std::string& str);
    std::string to_lower(const std::string& str);
}
