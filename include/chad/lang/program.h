#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "chad/lang/ast.h"

namespace chad {

using Breed = std::size_t;

// the breeds every program has
namespace breed {
constexpr Breed Number = 0;
constexpr Breed World = 1;
constexpr Breed Rep = 2;
constexpr Breed Ghost = 3;
constexpr Breed Say = 4;
constexpr Breed Hear = 5;
constexpr Breed Silence = 6;
constexpr Breed Cons = 7;
constexpr Breed Nil = 8;
}

struct BuiltinBreed {
    std::string_view name;
    std::size_t valueCount;
    std::size_t armCount;
};

// in id order
constexpr BuiltinBreed BUILTIN_BREEDS[] = {
    {"number", 1, 0},
    {"World", 0, 0},
    {"Rep", 0, 2},
    {"Ghost", 0, 0},
    {"Say", 1, 1},
    {"Hear", 0, 2},
    {"Silence", 0, 0},
    {"Cons", 0, 2},
    {"Nil", 0, 0},
};

struct BreedInfo {
    std::string name;
    std::size_t valueCount = 0;
    std::size_t armCount = 0;
    int line = 0; // where it was first seen. 0 for built-ins
};

// a value to compute when a rule fires
struct Code {
    enum class Kind {
        Constant,
        Slot,
        Unary,
        Binary
    };
    Kind kind = Kind::Constant;
    std::int64_t constant = 0;
    std::size_t slot = 0; // which of the meeting Chads' values
    Operator op = Operator::Add;
    std::vector<Code> operands;
    int line = 0;
};

// one end of a wire in a template
struct End {
    bool outside = false;
    std::size_t index = 0; // which outside port or which new Chad
    std::size_t slot = 0; // port of the new Chad: 0 is the face, 1 the arms
};

struct NewChad {
    Breed breed = 0;
    std::vector<Code> values;
    int line = 0;
};

// what a face-off turns into
struct Template {
    std::vector<NewChad> chads;
    std::vector<std::pair<End, End>> links;
};

struct RuleCase {
    bool hasCondition = false;
    Code condition;
    Template result;
    int line = 0;
};

struct Rule {
    Breed left = 0;
    Breed right = 0;
    std::vector<RuleCase> cases;
    int line = 0;
};

struct Program {
    std::vector<BreedInfo> breeds;
    std::vector<Rule> rules;
    std::vector<int> ruleTable;
    Template main;
    bool usesWorld = false;
    int mainLine = 0;

    int findRule(Breed a, Breed b) const {
        return ruleTable[a * breeds.size() + b];
    }
};

}