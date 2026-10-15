#include "chad/lang/parser.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include "chad/core/error.h"
#include "chad/lang/syntax.h"

namespace chad {

namespace {

using namespace ast;

struct OperatorSymbol {
    Symbol symbol;
    Operator op;
};

constexpr OperatorSymbol COMPARISONS[] = {
    {Symbol::Eq, Operator::Eq},
    {Symbol::Ne, Operator::Ne},
    {Symbol::Lt, Operator::Lt},
    {Symbol::Gt, Operator::Gt},
    {Symbol::Le, Operator::Le},
    {Symbol::Ge, Operator::Ge},
};

constexpr OperatorSymbol SUMS[] = {
    {Symbol::Plus, Operator::Add},
    {Symbol::Minus, Operator::Sub},
};

constexpr OperatorSymbol PRODUCTS[] = {
    {Symbol::Star, Operator::Mul},
    {Symbol::Slash, Operator::Div},
    {Symbol::Percent, Operator::Mod},
};

Expr unary(Operator op, Expr operand, int line) {
    Expr expr;
    expr.kind = ExprKind::Unary;
    expr.op = op;
    expr.line = line;
    expr.operands.push_back(std::move(operand));
    return expr;
}

Expr binary(Operator op, Expr left, Expr right, int line) {
    Expr expr;
    expr.kind = ExprKind::Binary;
    expr.op = op;
    expr.line = line;
    expr.operands.push_back(std::move(left));
    expr.operands.push_back(std::move(right));
    return expr;
}

class Parser {
public:
    explicit Parser(const TokenList& tokens) : tokens(tokens) {}

    Source run() {
        skipNewlines();

        while (!atEnd()) {
            if (isKeyword(Keyword::Main)) {
                parseMain();
            } else {
                parseRule();
            }

            skipNewlines();
        }

        return std::move(source);
    }

private:
    const TokenList& tokens;

    std::size_t pos = 0;
    Source source;

    const Token& peek() const {
        return tokens[pos];
    }

    bool atEnd() const {
        return peek().kind == TokenKind::End;
    }

    bool isSymbol(Symbol symbol) const {
        return peek().kind == TokenKind::Symbol && peek().symbol == symbol;
    }

    bool isKeyword(Keyword keyword) const {
        return peek().kind == TokenKind::Keyword && peek().keyword == keyword;
    }

    // the operator for the next token if it is one of these
    template <std::size_t N>
    std::optional<Operator> peekOperator(const OperatorSymbol (&operators)[N]) const {
        for (const auto& [symbol, op] : operators) {
            if (isSymbol(symbol)) return op;
        }
        return std::nullopt;
    }

    bool acceptSymbol(Symbol symbol) {
        if (!isSymbol(symbol)) return false;
        ++pos;
        return true;
    }

    [[noreturn]] void fail(const std::string& wanted) const {
        throw errorAt(peek().line, "expected " + wanted, "got " + describe(peek()));
    }

    void expectSymbol(Symbol symbol) {
        if (!acceptSymbol(symbol)) fail("`" + std::string(spelling(symbol)) + "`");
    }

    void skipNewlines() {
        while (peek().kind == TokenKind::Newline) {
            ++pos;
        }
    }

    void expectLineEnd() const {
        if (peek().kind != TokenKind::Newline && !atEnd()) fail("the end of the line");
    }

    bool lineHasVs() const {
        for (std::size_t i = pos; tokens[i].kind != TokenKind::Newline && tokens[i].kind != TokenKind::End; ++i) {
            if (tokens[i].kind == TokenKind::Keyword && tokens[i].keyword == Keyword::Vs) return true;
        }
        return false;
    }

    std::string takeName(const std::string& wanted) {
        if (peek().kind != TokenKind::Name) fail(wanted);
        return tokens[pos++].text;
    }

    // main until the end of the file
    void parseMain() {
        const int line = peek().line;

        ++pos;
        if (source.hasMain) {
            throw errorAt(line, "second `main`", "a program has one `main`");
        }
        source.hasMain = true;
        source.mainLine = line;

        while (true) {
            skipNewlines();

            if (atEnd() || isKeyword(Keyword::Main)) break;
            if (lineHasVs()) throw errorAt(peek().line, "rule after `main`", "rules go before `main`, which runs to the end of the file");

            for (Connection& connection : parseConnections()) {
                source.main.push_back(std::move(connection));
            }
            expectLineEnd();
        }
    }

    // Left vs Right => connections
    // Left vs Right then lines of `if condition => connections` and maybe a last `else => connections`
    void parseRule() {
        Rule rule;
        rule.line = peek().line;
        rule.left = parsePattern();

        if (!isKeyword(Keyword::Vs)) fail("`vs`");
        ++pos;
        rule.right = parsePattern();

        if (!isSymbol(Symbol::Arrow)) {
            expectLineEnd();
            skipNewlines();
        }

        if (acceptSymbol(Symbol::Arrow)) {
            Case single;
            single.line = rule.line;
            skipNewlines();
            single.connections = parseConnections();
            expectLineEnd();
            rule.cases.push_back(std::move(single));
            source.rules.push_back(std::move(rule));
            return;
        }

        if (!isKeyword(Keyword::If) && !isKeyword(Keyword::Else)) fail("`=>` or an `if` case");

        while (isKeyword(Keyword::If) || isKeyword(Keyword::Else)) {
            Case branch;
            branch.line = peek().line;

            const bool isElse = isKeyword(Keyword::Else);
            ++pos;

            if (!isElse) {
                branch.hasCondition = true;
                branch.condition = parseExpr();
            }

            expectSymbol(Symbol::Arrow);
            skipNewlines();
            branch.connections = parseConnections();
            expectLineEnd();
            rule.cases.push_back(std::move(branch));
            skipNewlines();

            if (isElse) {
                if (isKeyword(Keyword::If) || isKeyword(Keyword::Else)) {
                    throw errorAt(peek().line, "case after `else`", "`else` has to be the last case");
                }
                break;
            }
        }

        source.rules.push_back(std::move(rule));
    }

    // Breed[values](arms) or a name for a number Chad
    Pattern parsePattern() {
        Pattern pattern;
        pattern.line = peek().line;

        if (peek().kind == TokenKind::Name) {
            pattern.isNumber = true;
            pattern.values.push_back(tokens[pos++].text);
            return pattern;
        }

        if (peek().kind != TokenKind::Breed) fail("a Chad to match, like `Fib(r)` or `n`");

        pattern.breed = tokens[pos++].text;
        if (acceptSymbol(Symbol::LeftBracket)) {
            do {
                pattern.values.push_back(takeName("a value name"));
            } while (acceptSymbol(Symbol::Comma));
            expectSymbol(Symbol::RightBracket);
        }

        if (acceptSymbol(Symbol::LeftParen) && !acceptSymbol(Symbol::RightParen)) {
            do {
                pattern.arms.push_back(takeName("a wire name"));
            } while (acceptSymbol(Symbol::Comma));
            expectSymbol(Symbol::RightParen);
        }

        return pattern;
    }

    // a ~ b, c ~ d
    std::vector<Connection> parseConnections() {
        std::vector<Connection> connections;

        do {
            skipNewlines();

            Connection connection;
            connection.line = peek().line;
            connection.left = parseTerm();
            expectSymbol(Symbol::Tilde);
            skipNewlines();
            connection.right = parseTerm();
            connections.push_back(std::move(connection));
        } while (acceptSymbol(Symbol::Comma));

        return connections;
    }

    Term parseTerm() {
        Term term;
        term.line = peek().line;

        if (peek().kind == TokenKind::Breed) {
            term.kind = TermKind::Chad;
            term.name = tokens[pos++].text;

            if (acceptSymbol(Symbol::LeftBracket)) {
                do {
                    term.values.push_back(parseExpr());
                } while (acceptSymbol(Symbol::Comma));
                expectSymbol(Symbol::RightBracket);
            }

            if (acceptSymbol(Symbol::LeftParen) && !acceptSymbol(Symbol::RightParen)) {
                do {
                    term.arms.push_back(parseTerm());
                } while (acceptSymbol(Symbol::Comma));
                expectSymbol(Symbol::RightParen);
            }

            return term;
        }

        if (peek().kind == TokenKind::String) {
            term.kind = TermKind::String;
            term.text = tokens[pos++].chars;
            return term;
        }

        Expr value = parseExpr();
        if (value.kind == ExprKind::Name) {
            term.kind = TermKind::Name;
            term.name = value.name;
        } else {
            term.kind = TermKind::Value;
            term.value = std::move(value);
        }

        return term;
    }

    // lowest to highest: or, and, not, comparisons, + -, * / %, unary -
    Expr parseExpr() {
        Expr left = parseAnd();
        while (isKeyword(Keyword::Or)) {
            const int line = tokens[pos++].line;
            left = binary(Operator::Or, std::move(left), parseAnd(), line);
        }
        return left;
    }

    Expr parseAnd() {
        Expr left = parseNot();
        while (isKeyword(Keyword::And)) {
            const int line = tokens[pos++].line;
            left = binary(Operator::And, std::move(left), parseNot(), line);
        }
        return left;
    }

    Expr parseNot() {
        if (isKeyword(Keyword::Not)) {
            const int line = tokens[pos++].line;
            return unary(Operator::Not, parseNot(), line);
        }
        return parseComparison();
    }

    Expr parseComparison() {
        Expr left = parseSum();

        if (const std::optional<Operator> op = peekOperator(COMPARISONS)) {
            const int line = tokens[pos++].line;
            return binary(*op, std::move(left), parseSum(), line);
        }

        return left;
    }

    Expr parseSum() {
        Expr left = parseProduct();
        while (const std::optional<Operator> op = peekOperator(SUMS)) {
            const int line = tokens[pos++].line;
            left = binary(*op, std::move(left), parseProduct(), line);
        }
        return left;
    }

    Expr parseProduct() {
        Expr left = parseUnary();
        while (const std::optional<Operator> op = peekOperator(PRODUCTS)) {
            const int line = tokens[pos++].line;
            left = binary(*op, std::move(left), parseUnary(), line);
        }
        return left;
    }

    Expr parseUnary() {
        if (isSymbol(Symbol::Minus)) {
            const int line = tokens[pos++].line;
            return unary(Operator::Negate, parseUnary(), line);
        }
        return parsePrimary();
    }

    Expr parsePrimary() {
        Expr expr;
        expr.line = peek().line;

        if (peek().kind == TokenKind::Number || peek().kind == TokenKind::Char) {
            expr.kind = ExprKind::Number;
            expr.number = tokens[pos++].value;
            return expr;
        }

        if (acceptSymbol(Symbol::LeftParen)) {
            expr = parseExpr();
            expectSymbol(Symbol::RightParen);
            return expr;
        }

        expr.kind = ExprKind::Name;
        expr.name = takeName("a Chad, a wire or a value");

        return expr;
    }
};

}

ast::Source parse(const TokenList& tokens) {
    return Parser(tokens).run();
}

}