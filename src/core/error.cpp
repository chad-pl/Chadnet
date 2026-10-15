#include "chad/core/error.h"

#include <string>
#include <variant>

#include "chad/core/text.h"
#include "chad/lang/syntax.h"
#include "prelude.h"

namespace chad {

Error errorAt(int line, const std::string& kind, const std::string& detail) {
    std::string message = lineName(line) + ": " + kind;
    if (!detail.empty()) {
        message += " -- " + detail;
    }
    Error error(message);
    error.line = line;
    return error;
}

std::string lineName(int line) {
    if (line < 0) return preludeLineName(-line);

    return "line " + std::to_string(line);
}

std::string preludeLineName(int line) {
    for (const PreludeFile& file : PRELUDE_FILES) {
        const int lines = countLines(file.source);
        if (line <= lines) return std::string(file.name) + " line " + std::to_string(line);

        line -= lines;
    }

    return "prelude line " + std::to_string(line);
}

std::string plural(std::size_t count, const char* word) {
    if (count == 0) {
        return std::string("no ") + word + "s";
    } else if (count == 1) {
        return std::string("1 ") + word;
    }
    return std::to_string(count) + " " + word + "s";
}

std::string shape(std::size_t values, std::size_t arms) {
    return plural(values, "value") + " and " + plural(arms, "arm");
}

namespace {

// one overload per token kind, so a new kind without a description doesn't compile
struct TokenDescriber {
    std::string operator()(const token::Breed& breed) const {
        return quoted(breed.name);
    }

    std::string operator()(const token::Name& name) const {
        return quoted(name.name);
    }

    std::string operator()(const token::Keyword& keyword) const {
        return quoted(std::string(spelling(keyword.keyword)));
    }

    std::string operator()(const token::Symbol& symbol) const {
        return quoted(std::string(spelling(symbol.symbol)));
    }

    std::string operator()(const token::Number& number) const {
        return quoted(number.text);
    }

    std::string operator()(const token::Char& character) const {
        return quoted(character.text);
    }

    std::string operator()(const token::String&) const {
        return "a string";
    }

    std::string operator()(const token::Newline&) const {
        return "the end of the line";
    }

    std::string operator()(const token::End&) const {
        return "the end of the file";
    }

    static std::string quoted(const std::string& text) {
        return "`" + text + "`";
    }
};

}

std::string describe(const Token& token) {
    return std::visit(TokenDescriber{}, token.data);
}

}