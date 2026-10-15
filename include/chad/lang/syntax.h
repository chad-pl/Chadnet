#pragma once

#include <optional>
#include <string_view>

namespace chad {

// the words and symbols of the language

enum class Keyword {
    Main,
    Vs,
    If,
    Else,
    And,
    Or,
    Not,
};

enum class Symbol {
    Arrow, // =>
    Eq, // ==
    Ne, // !=
    Le, // <=
    Ge, // >=
    LeftParen,
    RightParen,
    LeftBracket,
    RightBracket,
    Comma,
    Tilde,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Lt,
    Gt,
};

constexpr std::string_view COMMENT_WORD = "ngl";
constexpr std::string_view WORLD_WIRE = "world";
constexpr std::string_view IGNORED_NAME = "_";

struct KeywordSpelling {
    Keyword keyword;
    std::string_view text;
};

constexpr KeywordSpelling KEYWORDS[] = {
    {Keyword::Main, "main"},
    {Keyword::Vs, "vs"},
    {Keyword::If, "if"},
    {Keyword::Else, "else"},
    {Keyword::And, "and"},
    {Keyword::Or, "or"},
    {Keyword::Not, "not"},
};

struct SymbolSpelling {
    Symbol symbol;
    std::string_view text;
};

// 2 character symbols first
constexpr SymbolSpelling SYMBOLS[] = {
    {Symbol::Arrow, "=>"},
    {Symbol::Eq, "=="},
    {Symbol::Ne, "!="},
    {Symbol::Le, "<="},
    {Symbol::Ge, ">="},
    {Symbol::LeftParen, "("},
    {Symbol::RightParen, ")"},
    {Symbol::LeftBracket, "["},
    {Symbol::RightBracket, "]"},
    {Symbol::Comma, ","},
    {Symbol::Tilde, "~"},
    {Symbol::Plus, "+"},
    {Symbol::Minus, "-"},
    {Symbol::Star, "*"},
    {Symbol::Slash, "/"},
    {Symbol::Percent, "%"},
    {Symbol::Lt, "<"},
    {Symbol::Gt, ">"},
};

std::optional<Keyword> findKeyword(std::string_view text);

std::string_view spelling(Keyword keyword);
std::string_view spelling(Symbol symbol);

}
