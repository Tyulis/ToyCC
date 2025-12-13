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

    void ltrim_inplace(std::string& str, std::string characters) {
        str.erase(0, str.find_first_not_of(characters));
    }

    void rtrim_inplace(std::string& str, std::string characters) {
        size_t trimmed_end = str.find_last_not_of(characters);
        if (trimmed_end != std::string::npos)
            str.erase(trimmed_end + 1);
    }

    void trim_inplace(std::string& str, std::string characters) {
        ltrim_inplace(str, characters);
        rtrim_inplace(str, characters);
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
}
