#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "chad/lang/syntax.h"

namespace chad {

namespace token {

struct Breed { // starts with a capital
    std::string name;
};

struct Name { // starts lowercase
    std::string name;
};

struct Keyword {
    chad::Keyword keyword;
};

struct Symbol { // ( ) [ ] , ~ => and operators
    chad::Symbol symbol;
};

struct Number {
    std::int64_t value;
    std::string text; // as written for error messages
};

struct Char {
    std::int64_t value; // code point
    std::string text;
};

struct String {
    std::vector<std::int64_t> chars; // code points
    std::string text;
};

struct Newline {};

struct End {};

}

using TokenData = std::variant<
    token::Breed,
    token::Name,
    token::Keyword,
    token::Symbol,
    token::Number,
    token::Char,
    token::String,
    token::Newline,
    token::End>;

struct Token {
    TokenData data;
    int line = 1;

    template <typename T>
    bool is() const {
        return std::holds_alternative<T>(data);
    }

    // nullptr when the token is another kind
    template <typename T>
    const T* as() const {
        return std::get_if<T>(&data);
    }
};

using TokenList = std::vector<Token>;

}