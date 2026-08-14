#pragma once

#include <memory>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::formula_language_detail {

enum class NodeKind { number, string_literal, symbol, unary, binary, call };

struct Node {
    NodeKind kind{NodeKind::number};
    double number{};
    std::string text;
    std::vector<std::unique_ptr<Node>> children;
};

struct Statement {
    std::string name;
    bool output{};
    std::unique_ptr<Node> expression;
    std::vector<std::string> directives;
};

struct Program {
    std::vector<Statement> statements;
    std::set<std::string> functions;
    std::set<std::string> symbols;
    std::set<std::string> assigned;
    std::vector<std::string> outputs;
    std::set<std::string> directives;
};

struct FormulaReferenceUse {
    std::string binding;
    std::vector<std::string> argument_expressions;
    bool scalar_arguments{};
};

Program parse_formula_program(std::string_view source);

void collect_numbered_context_calls(const Node& node,
                                    std::set<std::string>& required,
                                    std::set<std::string>& unavailable);

// Recover the exact TDX double-quoted indicator-output uses from the AST.
// Parameterized references are admitted only when every argument is a scalar
// arithmetic expression over constants and declared parent parameters.
void collect_formula_reference_uses(
    const Node& node,
    const std::set<std::string>& parameters,
    std::vector<FormulaReferenceUse>& uses);

// Evaluate one expression emitted by collect_formula_reference_uses.  This is
// intentionally scalar-only: market series are not silently collapsed to a
// last-bar value.
double evaluate_formula_scalar_expression(
    std::string_view expression,
    const std::map<std::string, double>& parameters);

}  // namespace tdx::formula_language_detail
