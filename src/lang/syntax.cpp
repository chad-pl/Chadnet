#include "chad/lang/syntax.h"

namespace chad {

std::optional<Keyword> findKeyword(std::string_view text) {
    for (const KeywordSpelling& entry : KEYWORDS) {
        if (entry.text == text) return entry.keyword;
    }
    return std::nullopt;
}

std::string_view spelling(Keyword keyword) {
    for (const KeywordSpelling& entry : KEYWORDS) {
        if (entry.keyword == keyword) return entry.text;
    }
    return {};
}

std::string_view spelling(Symbol symbol) {
    for (const SymbolSpelling& entry : SYMBOLS) {
        if (entry.symbol == symbol) return entry.text;
    }
    return {};
}

}
