#pragma once

#include <ranges>
#include <string>
#include <vector>
#include <sstream>

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

    std::string& unescape_inplace(std::string& str);
    std::string unescape(const std::string& str);

    std::string& xml_escape_inplace(std::string& str);
    std::string xml_escape(const std::string& str);

    std::string indent(const std::string& str, bool indent_first_line=true, std::string prefix="\t");

    std::string& to_lower_inplace(std::string& str);
    std::string to_lower(const std::string& str);

    std::string& justify_right_inplace(std::string& str, size_t length, char padding=' ');
    std::string justify_right(const std::string& str, size_t length, char padding=' ');

    std::string& center_inplace(std::string& str, size_t length, char padding=' ');
    std::string center(const std::string& str, size_t length, char padding=' ');

    template <typename T> requires requires (std::ostream& stream, const T& value) { {stream << value} -> std::convertible_to<std::ostream&>; }
    std::string dump(const T& value) {
        std::stringstream stream;
        stream << value;
        return stream.str();
    }

    template <typename R> requires std::ranges::input_range<R> && std::ranges::sized_range<R>
    std::string join(R&& range, const std::string& delimiter) {
        std::stringstream output;
        for (const auto& [index, item] : std::ranges::enumerate_view(range)) {
            output << item;
            if (static_cast<size_t>(index) != std::ranges::size(range) - 1)
                output << delimiter;
        }
        return output.str();
    }
}
