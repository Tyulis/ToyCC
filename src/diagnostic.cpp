#include <format>
#include <string>
#include <fstream>
#include <sstream>

#include "diagnostic.h"
#include "util/strings.h"

namespace toycc {
    Diagnostic::Diagnostic(Level level, std::string base_message, std::optional<std::string> filename, std::optional<size_t> line, std::optional<size_t> character)
        : _level(level), _base_message(base_message), _filename(filename), _line(line), _character(character) {}

    Diagnostic::Diagnostic(Level level, std::string base_message, CodeLocation location)
        : _level(level), _base_message(base_message), _filename(location.filename), _line(location.line), _character(location.character) {}

    Diagnostic& Diagnostic::add_note(Diagnostic note) {
        notes.push_back(note);
        return *this;
    }

    Diagnostic& Diagnostic::add_note(Level level, std::string base_message, std::optional<std::string> filename, std::optional<size_t> line, std::optional<size_t> character) {
        notes.emplace_back(level, base_message, filename, line, character);
        return *this;
    }

    Diagnostic& Diagnostic::add_note(Level level, std::string base_message, CodeLocation location) {
        notes.emplace_back(level, base_message, location);
        return *this;
    }

    Diagnostic::Level          Diagnostic::level()        const { return _level;        }
    std::string                Diagnostic::base_message() const { return _base_message; }
    std::optional<std::string> Diagnostic::filename()     const { return _filename;     }
    std::optional<size_t>      Diagnostic::line()         const { return _line;         }
    std::optional<size_t>      Diagnostic::character()    const { return _character;    }

    Diagnostic& Diagnostic::level(Diagnostic::Level level) {
        _level = level;
        return *this;
    }

    Diagnostic& Diagnostic::base_message(std::string base_message) {
        _base_message = base_message;
        return *this;
    }

    Diagnostic& Diagnostic::filename(std::string filename) {
        _filename = filename;
        return *this;
    }

    Diagnostic& Diagnostic::line(size_t line) {
        _line = line;
        return *this;
    }

    Diagnostic& Diagnostic::character(size_t character) {
        _character = character;
        return *this;
    }

    std::string Diagnostic::own_message() const {
        std::string level_text;
        switch (_level) {
            case Level::DEBUG:           level_text = "DEBUG";           break;
            case Level::NOTE:            level_text = "NOTE";            break;
            case Level::WARNING:         level_text = "WARNING";         break;
            case Level::ERROR:           level_text = "ERROR";           break;
            case Level::INTERNAL_ERROR:  level_text = "INTERNAL_ERROR";  break;
            case Level::NOT_IMPLEMENTED: level_text = "NOT_IMPLEMENTED"; break;
        }

        if (!_filename.has_value() && !_line.has_value()) {  // No location information -> don't print it'
            std::string message = std::format("{} : {}", level_text, _base_message);
            return message;
        }


        std::string message = std::format("{} at {}:{}:{} : {}", level_text, _filename.value_or("<input>"), _line.value_or(0) + 1, _character.value_or(0) + 1, _base_message);
        if (!_filename.has_value() || !_line.has_value())
            return message;

        std::ifstream source_file(_filename.value());
        std::string source_line;

        if (!source_file.is_open())
            return message;

        for (unsigned line = 0; line < _line.value() && !source_file.eof(); line++)
            std::getline(source_file, source_line);

        if (source_line.empty())
            return message;

        message += std::format("\n\t{}", source_line);
        if (!_character.has_value())
            return message;

        message += "\n\t";
        for (unsigned character = 0; character < _character.value(); character++)
            message += "-";
        message += "^";

        return message;
    }

    std::string Diagnostic::message() const {
        std::stringstream full_message;
        full_message << own_message();
        for (const Diagnostic& note : notes)
            full_message << "\n" << indent(note.message());
        return full_message.str();
    }

    std::ostream& operator<< (std::ostream& stream, Diagnostic diagnostic) {
        stream << diagnostic.message();
        return stream;
    }
}
