#pragma once

#include <string>
#include <vector>

namespace toycc {
    constexpr std::string WHITESPACE = " \r\n\t\v";

    std::vector<std::string> split(const std::string& str, const std::string& delimiter);
    std::vector<std::string> split_one_of(const std::string& str, const std::string& delimiters = WHITESPACE);

    void ltrim_inplace(std::string& str, std::string characters = WHITESPACE);
    void rtrim_inplace(std::string& str, std::string characters = WHITESPACE);
    void trim_inplace (std::string& str, std::string characters = WHITESPACE);

    std::string ltrim(const std::string& str, std::string characters = WHITESPACE);
    std::string rtrim(const std::string& str, std::string characters = WHITESPACE);
    std::string trim (const std::string& str, std::string characters = WHITESPACE);
}
