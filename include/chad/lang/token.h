#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "chad/lang/syntax.h"

namespace chad {

enum class TokenKind {
    Breed, // starts with a capital
    Name, // starts lowercase
    Keyword, // a reserved word
    Number,
    Char,
    String,
    Symbol, // ( ) [ ] , ~ => and operators
    Newline,
    End,
};

struct Token {
    TokenKind kind = TokenKind::End;
    std::string text;
    std::int64_t value = 0; // Number and Char
    std::vector<std::int64_t> chars; // String, as code points
    Keyword keyword = Keyword::Main;
    Symbol symbol = Symbol::Arrow;
    int line = 1;
};

using TokenList = std::vector<Token>;

}