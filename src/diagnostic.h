#pragma once

#include <string>
#include <vector>
#include <optional>

#include "code_location.h"

namespace toycc {
    class Diagnostic {
        public:
            enum class Level : int {
                DEBUG, NOTE, WARNING, ERROR, INTERNAL_ERROR, NOT_IMPLEMENTED,
            };

            Diagnostic(Level level, std::string base_message, std::optional<std::string> filename = {}, std::optional<size_t> line = {}, std::optional<size_t> character = {});
            Diagnostic(Level level, std::string base_message, CodeLocation location);

            Diagnostic& add_note(Diagnostic note);
            Diagnostic& add_note(Level level, std::string base_message, std::optional<std::string> filename = {}, std::optional<size_t> line = {}, std::optional<size_t> character = {});
            Diagnostic& add_note(Level level, std::string base_message, CodeLocation location);

            Level level() const;
            std::string base_message() const;
            std::optional<std::string> filename() const;
            std::optional<size_t> line() const;
            std::optional<size_t> character() const;
            std::string message() const;

            Diagnostic& level(Level level);
            Diagnostic& base_message(std::string base_message);
            Diagnostic& filename(std::string filename);
            Diagnostic& line(size_t line);
            Diagnostic& character(size_t character);

        private:
            Level _level;
            std::string _base_message;
            std::optional<std::string> _filename;
            std::optional<size_t> _line;
            std::optional<size_t> _character;

            std::vector<Diagnostic> notes;

            std::string own_message() const;
    };

    std::ostream& operator<< (std::ostream& stream, Diagnostic diagnostic);
}
