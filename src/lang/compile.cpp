#include "chad/lang/compile.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "chad/core/error.h"
#include "chad/core/text.h"
#include "chad/lang/ast.h"
#include "chad/lang/lexer.h"
#include "chad/lang/parser.h"
#include "chad/lang/syntax.h"
#include "prelude.h"

namespace chad {

namespace {

struct Scope {
    std::map<std::string, std::size_t> values;
    std::map<std::string, std::size_t> outside;

    std::size_t valueCount = 0;
    std::size_t outsideCount = 0;

    bool inMain = false;
};

class Compiler {
public:
    Compiler() {
        for (const BuiltinBreed& builtin : BUILTIN_BREEDS) {
            const std::string name(builtin.name);
            if (program.breeds.size() != breed::Number) {
                breedIds.emplace(name, program.breeds.size());
            }
            program.breeds.push_back({name, builtin.valueCount, builtin.armCount, 0});
        }
    }

    void addPreludeRules(const ast::Source& source, int lineOffset) {
        inPrelude = true;
        preludeOffset = lineOffset;
        for (const ast::Rule& rule : source.rules) {
            addRule(rule);
        }
    }

    void addRules(const ast::Source& source) {
        inPrelude = false;
        for (const ast::Rule& rule : source.rules) {
            addRule(rule);
        }
    }

    void addMain(const ast::Source& source);

    Program finish() {
        const std::size_t count = program.breeds.size();
        program.ruleTable.assign(count * count, -1);

        for (std::size_t i = 0; i < program.rules.size(); i++) {
            const Rule& rule = program.rules[i];
            program.ruleTable[rule.left * count + rule.right] = static_cast<int>(i);
            program.ruleTable[rule.right * count + rule.left] = static_cast<int>(i);
        }

        return std::move(program);
    }

    int at(int line) const {
        return inPrelude ? -(preludeOffset + line) : line;
    }

    // every use of a breed must have the same number of values and arms as the first one
    Breed breedFor(const std::string& name, std::size_t valueCount, std::size_t armCount, int line) {
        line = at(line);
        if (name == BUILTIN_BREEDS[breed::World].name) throw errorAt(line, "`World` belongs to the runtime", "your program gets it through the `world` wire in `main`");

        const auto found = breedIds.find(name);
        if (found == breedIds.end()) {
            const Breed id = program.breeds.size();
            program.breeds.push_back({name, valueCount, armCount, line});
            breedIds.emplace(name, id);
            return id;
        }

        const BreedInfo& info = program.breeds[found->second];
        if (info.valueCount != valueCount || info.armCount != armCount) {
            const std::string where = info.line == 0 ? "as a built-in" : "on " + lineName(info.line);
            throw errorAt(line, "wrong shape for `" + name + "`", "it has " + shape(valueCount, armCount) + " here, but " + shape(info.valueCount, info.armCount) + " " + where);
        }

        return found->second;
    }

    Code compileExpr(const ast::Expr& expr, const Scope& scope) const {
        Code code;
        code.line = at(expr.line);

        switch (expr.kind) {
            case ast::ExprKind::Number:
                code.kind = Code::Kind::Constant;
                code.constant = expr.number;
                return code;
            case ast::ExprKind::Name: {
                const auto value = scope.values.find(expr.name);
                if (value != scope.values.end()) {
                    code.kind = Code::Kind::Slot;
                    code.slot = value->second;
                    return code;
                } else if (expr.name == IGNORED_NAME) {
                    throw errorAt(code.line, "`_` only works in rule heads");
                } else if (scope.outside.count(expr.name) != 0 || expr.name == WORLD_WIRE) {
                    throw errorAt(code.line, "`" + expr.name + "` is a wire, not a value", "values are the names in `[ ]` or a number in the rule head");
                }
                throw errorAt(code.line, "unknown value `" + expr.name + "`");
            }
            case ast::ExprKind::Unary:
            case ast::ExprKind::Binary:
                code.kind = expr.kind == ast::ExprKind::Unary ? Code::Kind::Unary : Code::Kind::Binary;
                code.op = expr.op;
                for (const ast::Expr& operand : expr.operands) {
                    code.operands.push_back(compileExpr(operand, scope));
                }
                return code;
        }

        return code;
    }

private:
    Program program;
    std::map<std::string, Breed> breedIds;
    std::map<std::pair<Breed, Breed>, int> ruleLines; // each pair of breeds
    bool inPrelude = false;
    int preludeOffset = 0;

    Breed patternBreed(const ast::Pattern& pattern) {
        if (pattern.isNumber) return breed::Number;
        return breedFor(pattern.breed, pattern.values.size(), pattern.arms.size(), pattern.line);
    }

    void bind(const ast::Pattern& pattern, Scope& scope) const {
        for (const std::string& name : pattern.values) {
            if (name != IGNORED_NAME) {
                scope.values.emplace(name, scope.valueCount);
            }
            scope.valueCount++;
        }

        for (const std::string& name : pattern.arms) {
            if (name == IGNORED_NAME) {
                throw errorAt(at(pattern.line), "an arm can't be `_`", "every wire needs two ends; to drop something, wire it to `Ghost`");
            }
            if (name == WORLD_WIRE) throw errorAt(at(pattern.line), "`world` only exists in `main`", "pass the World along through arms, like `Print(w, next)`");
            scope.outside.emplace(name, scope.outsideCount++);
        }
    }

    std::string breedName(Breed breed) const {
        return program.breeds[breed].name;
    }

    void checkPair(const ast::Rule& rule, Breed left, Breed right) const {
        for (const Breed side : {left, right}) {
            if (side == breed::Rep) {
                throw errorAt(at(rule.line), "`Rep` rules are built in", "`Rep` copies whatever it meets");
            } else if (side == breed::Ghost) {
                throw errorAt(at(rule.line), "`Ghost` rules are built in", "`Ghost` erases whatever it meets");
            } else if (side == breed::Say || side == breed::Hear || side == breed::World) {
                throw errorAt(at(rule.line), "`" + breedName(side) + "` only meets the World", "it can't have rules");
            }
        }

        if (left == breed::Number && right == breed::Number) {
            throw errorAt(at(rule.line), "a rule needs a breed", "two numbers can't meet");
        } else if (left == right && !inPrelude) {
            throw errorAt(at(rule.line), "`" + breedName(left) + "` can't have a rule with itself", "both sides would be the same breed, so the result could depend on which one is which");
        }
    }

    void addRule(const ast::Rule& rule);
};

class TemplateBuilder {
public:
    TemplateBuilder(Compiler& compiler, const Scope& scope, int headLine) : compiler(compiler), scope(scope) {
        for (const auto& [name, port] : scope.outside) {
            Wire& wire = wires[wireFor(name)];
            wire.fromHead = true;
            wire.uses.push_back(headLine);
            wire.ends.push_back(End{true, port, 0});
        }
    }

    Template build(const std::vector<ast::Connection>& connections) {
        for (const ast::Connection& connection : connections) {
            connect(connection);
        }
        finishWires();

        return std::move(result);
    }

    bool usedWorld() const {
        return hasWorld;
    }

private:
    struct Wire {
        std::string name;
        std::vector<int> uses;
        std::vector<End> ends;
        bool fromHead = false;
        std::size_t joinedTo = 0;
    };

    struct Placed {
        bool isWire = false;
        std::size_t wire = 0;
        End end;
    };

    Compiler& compiler;
    const Scope& scope;
    Template result;
    std::vector<Wire> wires;
    std::map<std::string, std::size_t> wireIds;
    bool hasWorld = false;
    std::size_t worldWire = 0;

    std::size_t wireFor(const std::string& name) {
        const auto [found, isNew] = wireIds.emplace(name, wires.size());
        if (isNew) {
            Wire wire;
            wire.name = name;
            wire.joinedTo = wires.size();
            wires.push_back(std::move(wire));
        }

        return found->second;
    }

    std::size_t joinedRoot(std::size_t wire) const {
        while (wires[wire].joinedTo != wire) {
            wire = wires[wire].joinedTo;
        }

        return wire;
    }

    static End face(std::size_t chad) {
        return End{false, chad, 0};
    }

    static Placed wirePlaced(std::size_t wire) {
        Placed placed;
        placed.isWire = true;
        placed.wire = wire;
        return placed;
    }

    static Placed endPlaced(End end) {
        Placed placed;
        placed.end = end;
        return placed;
    }

    std::size_t newChad(Breed breed, std::vector<Code> values, int line) {
        NewChad chad;
        chad.breed = breed;
        chad.values = std::move(values);
        chad.line = compiler.at(line);
        result.chads.push_back(std::move(chad));

        return result.chads.size() - 1;
    }

    std::size_t numberChad(Code value, int line) {
        std::vector<Code> values;
        values.push_back(std::move(value));

        return newChad(breed::Number, std::move(values), line);
    }

    Placed place(const ast::Term& term) {
        switch (term.kind) {
            case ast::TermKind::Name:
                return placeName(term);
            case ast::TermKind::Value:
                return endPlaced(face(numberChad(compiler.compileExpr(term.value, scope), term.line)));
            case ast::TermKind::Chad:
                return endPlaced(face(placeChad(term)));
            case ast::TermKind::String:
                return endPlaced(placeString(term));
        }

        return Placed{};
    }

    Placed placeName(const ast::Term& term) {
        const std::string& name = term.name;
        if (name == IGNORED_NAME) throw errorAt(compiler.at(term.line), "`_` only works in rule heads");

        const auto value = scope.values.find(name);
        if (value != scope.values.end()) {
            Code code;
            code.kind = Code::Kind::Slot;
            code.slot = value->second;
            code.line = compiler.at(term.line);
            return endPlaced(face(numberChad(std::move(code), term.line)));
        }

        if (name == WORLD_WIRE) {
            if (!scope.inMain) throw errorAt(compiler.at(term.line), "`world` only exists in `main`", "pass the World along through arms, like `Print(w, next)`");

            if (!hasWorld) {
                hasWorld = true;
                worldWire = wireFor(name);
                attach(worldWire, End{true, 0, 0}, term.line);
            }
        }

        return wirePlaced(wireFor(name));
    }

    std::size_t placeChad(const ast::Term& term) {
        const Breed breed = compiler.breedFor(term.name, term.values.size(), term.arms.size(), term.line);

        std::vector<Code> values;
        for (const ast::Expr& value : term.values) {
            values.push_back(compiler.compileExpr(value, scope));
        }

        const std::size_t chad = newChad(breed, std::move(values), term.line);
        for (std::size_t i = 0; i < term.arms.size(); i++) {
            plug(End{false, chad, i + 1}, place(term.arms[i]), term.arms[i].line);
        }

        return chad;
    }

    End placeString(const ast::Term& term) {
        std::size_t first = 0;
        std::size_t previous = 0;
        bool empty = true;

        for (const std::int64_t c : term.text) {
            Code code;
            code.constant = c;
            code.line = compiler.at(term.line);

            const std::size_t cons = newChad(breed::Cons, {}, term.line);
            result.links.push_back({End{false, cons, 1}, face(numberChad(std::move(code), term.line))});
            if (empty) {
                first = cons;
            } else {
                result.links.push_back({End{false, previous, 2}, face(cons)});
            }
            previous = cons;
            empty = false;
        }

        const std::size_t nil = newChad(breed::Nil, {}, term.line);
        if (empty) return face(nil);

        result.links.push_back({End{false, previous, 2}, face(nil)});
        return face(first);
    }

    void attach(std::size_t wire, End end, int line) {
        wires[wire].uses.push_back(line);
        wires[wire].ends.push_back(end);
    }

    void plug(End position, const Placed& placed, int line) {
        if (placed.isWire) {
            attach(placed.wire, position, line);
        } else {
            result.links.push_back({position, placed.end});
        }
    }

    void connect(const ast::Connection& connection) {
        const Placed left = place(connection.left);
        const Placed right = place(connection.right);

        if (left.isWire && right.isWire) {
            wires[left.wire].uses.push_back(connection.line);
            wires[right.wire].uses.push_back(connection.line);

            const std::size_t a = joinedRoot(left.wire);
            const std::size_t b = joinedRoot(right.wire);
            wires[a].joinedTo = b;
        } else if (left.isWire) {
            attach(left.wire, right.end, connection.line);
        } else if (right.isWire) {
            attach(right.wire, left.end, connection.line);
        } else {
            if (isNumber(left.end) && isNumber(right.end)) throw errorAt(compiler.at(connection.line), "two numbers can't meet", "one side needs a breed");
            result.links.push_back({left.end, right.end});
        }
    }

    bool isNumber(End end) const {
        return !end.outside && end.slot == 0 && result.chads[end.index].breed == breed::Number;
    }

    void finishWires() {
        for (std::size_t i = 0; i < wires.size(); i++) {
            const Wire& wire = wires[i];
            const std::size_t count = wire.uses.size();
            if (count == 2) continue;

            if (hasWorld && i == worldWire) throw errorAt(compiler.at(wire.uses[2]), "`world` used twice", "`main` holds one end of the World's wire");

            if (count == 1 && wire.fromHead) throw errorAt(compiler.at(wire.uses[0]), "loose wire `" + wire.name + "`", "it comes from the rule head but is never connected");
            if (count == 1) throw errorAt(compiler.at(wire.uses[0]), "loose wire `" + wire.name + "`", "it has one end; every wire needs two");

            throw errorAt(compiler.at(wire.uses[2]), "wire `" + wire.name + "` used " + std::to_string(count) + " times", "every wire has exactly two ends");
        }

        std::map<std::size_t, std::vector<End>> joined;
        for (std::size_t i = 0; i < wires.size(); i++) {
            std::vector<End>& ends = joined[joinedRoot(i)];
            ends.insert(ends.end(), wires[i].ends.begin(), wires[i].ends.end());
        }

        for (const auto& [root, ends] : joined) {
            if (ends.size() == 2) {
                result.links.push_back({ends[0], ends[1]});
            } else if (ends.empty()) {
                throw errorAt(compiler.at(wires[root].uses[0]), "wire loop `" + wires[root].name + "`", "it only connects to itself");
            }
        }
    }
};

void Compiler::addRule(const ast::Rule& rule) {
    const Breed left = patternBreed(rule.left);
    const Breed right = patternBreed(rule.right);
    checkPair(rule, left, right);

    const auto [found, isNew] = ruleLines.emplace(std::minmax(left, right), at(rule.line));
    if (!isNew) throw errorAt(at(rule.line), "second rule for `" + breedName(left) + " vs " + breedName(right) + "`", "the first one is on " + lineName(found->second));

    Scope scope;
    bind(rule.left, scope);
    bind(rule.right, scope);

    Rule compiled;
    compiled.left = left;
    compiled.right = right;
    compiled.line = at(rule.line);

    for (const ast::Case& branch : rule.cases) {
        RuleCase compiledCase;
        compiledCase.line = at(branch.line);
        if (branch.hasCondition) {
            compiledCase.hasCondition = true;
            compiledCase.condition = compileExpr(branch.condition, scope);
        }
        compiledCase.result = TemplateBuilder(*this, scope, rule.line).build(branch.connections);
        compiled.cases.push_back(std::move(compiledCase));
    }

    program.rules.push_back(std::move(compiled));
}

void Compiler::addMain(const ast::Source& source) {
    inPrelude = false;
    if (!source.hasMain) throw errorAt(1, "no `main`", "a program starts from its `main` block");

    Scope scope;
    scope.inMain = true;

    TemplateBuilder builder(*this, scope, source.mainLine);
    program.main = builder.build(source.main);
    program.usesWorld = builder.usedWorld();
    program.mainLine = source.mainLine;
}
}

Program compile(const std::string& source) {
    Compiler compiler;

    int lineOffset = 0;
    for (const PreludeFile& file : PRELUDE_FILES) {
        compiler.addPreludeRules(parse(lex(file.source)), lineOffset);
        lineOffset += countLines(file.source);
    }

    const ast::Source program = parse(lex(source));

    compiler.addRules(program);
    compiler.addMain(program);

    return compiler.finish();
}
}