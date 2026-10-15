#include "chad/lang/lexer.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "chad/core/error.h"
#include "chad/core/text.h"
#include "chad/lang/syntax.h"
#include "chad/lang/token.h"
#include "chad/runtime/utf8.h"

namespace chad {

namespace {

const std::string UTF8_BOM = "\xEF\xBB\xBF"; // some editors (notepad) put these bytes to mark UTF-8

bool isNameStart(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool isNameChar(char c) {
    return isNameStart(c) || isDigit(c);
}

class Lexer {
public:
    explicit Lexer(const std::string& source) : source(source) {}

    TokenList run() {
        if (source.compare(0, UTF8_BOM.size(), UTF8_BOM) == 0) {
            index = UTF8_BOM.size();
        }

        while (index < source.size()) {
            const char c = source[index];
            if (c == '\n' || c == '\r') {
                newline();
            } else if (isSpace(c)) {
                ++index;
            } else if (isNameStart(c)) {
                name();
            } else if (isDigit(c)) {
                number();
            } else if (c == '\'') {
                character();
            } else if (c == '"') {
                string();
            } else {
                symbol();
            }
        }

        push(token::End{});
        return tokens;
    }

private:
    const std::string& source;
    std::size_t index = 0;
    int line = 1;
    int depth = 0; // open ( and [ where a line break doesn't end anything
    TokenList tokens;

    void push(TokenData data) {
        tokens.push_back(Token{std::move(data), line});
    }

    bool atLineEnd() const {
        return index >= source.size() || source[index] == '\n' || source[index] == '\r';
    }

    void newline() {
        const bool windowsLineEnd = source[index] == '\r' && index + 1 < source.size() && source[index + 1] == '\n';
        index += windowsLineEnd ? 2 : 1;

        if (depth == 0 && !tokens.empty() && !tokens.back().is<token::Newline>()) {
            push(token::Newline{});
        }
        ++line;
    }

    void name() {
        const std::size_t start = index;
        while (index < source.size() && isNameChar(source[index])) {
            ++index;
        }

        std::string text = source.substr(start, index - start);
        if (text == COMMENT_WORD) {
            while (!atLineEnd()) {
                ++index;
            }
            return;
        }

        const std::optional<Keyword> keyword = findKeyword(text);
        if (keyword) {
            push(token::Keyword{*keyword});
        } else if (text[0] >= 'A' && text[0] <= 'Z') {
            push(token::Breed{std::move(text)});
        } else {
            push(token::Name{std::move(text)});
        }
    }

    void number() {
        const std::size_t start = index;
        // unsigned so a literal too big for 64 bits wraps instead of overflowing
        std::uint64_t value = 0;
        while (index < source.size() && isDigit(source[index])) {
            value = value * 10 + static_cast<std::uint64_t>(source[index] - '0');
            ++index;
        }

        if (index < source.size() && isNameChar(source[index])) {
            while (index < source.size() && isNameChar(source[index])) {
                ++index;
            }
            throw errorAt(line, "bad number `" + source.substr(start, index - start) + "`", "names can't start with a digit");
        }

        push(token::Number{static_cast<std::int64_t>(value), source.substr(start, index - start)});
    }

    void character() {
        const std::size_t start = index++;
        if (index < source.size() && source[index] == '\'') {
            throw errorAt(line, "empty character", "`''` has nothing in it");
        }

        const std::int64_t value = readChar("character");
        if (index >= source.size() || source[index] != '\'') {
            throw errorAt(line, "unclosed character", "a character literal holds one character, like `'a'`");
        }
        ++index;

        push(token::Char{value, source.substr(start, index - start)});
    }

    void string() {
        const std::size_t start = index++;

        std::vector<std::int64_t> chars;
        while (index < source.size() && source[index] != '"') {
            chars.push_back(readChar("string"));
        }

        if (index >= source.size()) {
            throw errorAt(line, "unclosed string", "missing the closing `\"`");
        }

        ++index;

        push(token::String{std::move(chars), source.substr(start, index - start)});
    }

    // one character inside a '' or "" literal, escapes included
    std::int64_t readChar(const char* literal) {
        if (atLineEnd()) {
            throw errorAt(line, std::string("unclosed ") + literal, "literals end on the line they start");
        }

        if (source[index] == '\\') {
            ++index;

            if (atLineEnd()) {
                throw errorAt(line, std::string("unclosed ") + literal, "`\\` at the end of the line");
            }

            const char escaped = source[index++];
            switch (escaped) {
                case 'n':
                    return '\n';
                case 't':
                    return '\t';
                case 'r':
                    return '\r';
                case '0':
                    return 0;
                case '\\':
                    return '\\';
                case '\'':
                    return '\'';
                case '"':
                    return '"';
                default:
                    throw errorAt(line, std::string("unknown escape `\\") + escaped + "`", "use \\n \\t \\r \\0 \\\\ \\' or \\\"");
            }
        }

        const unsigned char firstByte = static_cast<unsigned char>(source[index]);
        const std::size_t byteCount = utf8::charByteCount(firstByte);
        if (byteCount == 0 || index + byteCount > source.size()) {
            throw errorAt(line, "invalid UTF-8", std::string("in a ") + literal);
        } else if (byteCount == 1) {
            ++index;
            return firstByte;
        }

        std::int64_t codePoint = utf8::firstByteBits(firstByte, byteCount);
        for (std::size_t i = 1; i < byteCount; ++i) {
            const unsigned char nextByte = static_cast<unsigned char>(source[index + i]);
            if (!utf8::isContinuationByte(nextByte)) {
                throw errorAt(line, "invalid UTF-8", std::string("in a ") + literal);
            }
            codePoint = (codePoint << 6) | (nextByte & 0x3F);
        }

        index += byteCount;

        return codePoint;
    }

    void symbol() {
        for (const auto& [symbol, text] : SYMBOLS) {
            if (source.compare(index, text.size(), text) != 0) continue;

            if (symbol == Symbol::LeftParen || symbol == Symbol::LeftBracket) {
                ++depth;
            } else if ((symbol == Symbol::RightParen || symbol == Symbol::RightBracket) && depth > 0) {
                --depth;
            }

            index += text.size();

            push(token::Symbol{symbol});

            return;
        }

        const char c = source[index];
        if (c == '=') {
            throw errorAt(line, "unknown symbol `=`", "use `==` to compare, or `=>` in a rule");
        }
        throw errorAt(line, "unknown symbol `" + std::string(1, c) + "`");
    }
};

}

TokenList lex(const std::string& source) {
    return Lexer(source).run();
}

}