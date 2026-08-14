#include "formula_language_internal.hpp"
#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <memory>
#include <map>
#include <limits>
#include <sstream>
#include <iomanip>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::formula_language_detail {

std::string upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return ch < 0x80 ? static_cast<char>(std::toupper(ch))
                         : static_cast<char>(ch);
    });
    return value;
}

enum class TokenKind {
    end, identifier, number, string_literal, plus, minus, star, slash, percent, left_paren,
    right_paren, comma, semicolon, colon, assign, equal, not_equal, less,
    less_equal, greater, greater_equal, logical_and, logical_or, logical_not
};

struct Token {
    TokenKind kind{TokenKind::end};
    std::string text;
    double number{};
    std::size_t offset{};
};

bool identifier_start(unsigned char ch) {
    return std::isalpha(ch) || ch == '_' || ch >= 0x80;
}

bool identifier_part(unsigned char ch) {
    return std::isalnum(ch) || ch == '_' || ch == '$' || ch == '#' || ch == '.' || ch >= 0x80;
}

std::vector<Token> lex(std::string_view source) {
    std::vector<Token> tokens;
    std::size_t at = 0;
    auto push = [&](TokenKind kind, std::size_t start, std::size_t size) {
        tokens.push_back(Token{kind, std::string(source.substr(start, size)), 0.0, start});
    };
    while (at < source.size()) {
        const auto ch = static_cast<unsigned char>(source[at]);
        if (std::isspace(ch)) { ++at; continue; }
        if (ch == '\'' || ch == '"') {
            const char quote = static_cast<char>(ch); const auto start = at++; std::string value;
            while (at < source.size()) {
                if (source[at] == quote) {
                    if (at + 1 < source.size() && source[at + 1] == quote) { value.push_back(quote); at += 2; continue; }
                    ++at; break;
                }
                value.push_back(source[at++]);
            }
            if (at > source.size() || source[at - 1] != quote)
                throw Error("unterminated formula string at byte " + std::to_string(start));
            if (quote == '"') tokens.push_back(Token{TokenKind::identifier, "EXTERNAL#" + value, 0.0, start});
            else tokens.push_back(Token{TokenKind::string_literal, value, 0.0, start});
            continue;
        }
        if (ch == '{') {
            const auto end = source.find('}', at + 1);
            if (end == std::string_view::npos)
                throw Error("unterminated formula comment at byte " + std::to_string(at));
            at = end + 1;
            continue;
        }
        if (ch == '/' && at + 1 < source.size() && source[at + 1] == '/') {
            const auto end = source.find_first_of("\r\n", at + 2);
            at = end == std::string_view::npos ? source.size() : end;
            continue;
        }
        if (identifier_start(ch)) {
            const auto start = at++;
            while (at < source.size() && identifier_part(static_cast<unsigned char>(source[at]))) ++at;
            auto text = std::string(source.substr(start, at - start));
            const auto keyword = upper_ascii(text);
            if (keyword == "AND") tokens.push_back(Token{TokenKind::logical_and, text, 0.0, start});
            else if (keyword == "OR") tokens.push_back(Token{TokenKind::logical_or, text, 0.0, start});
            else if (keyword == "NOT") tokens.push_back(Token{TokenKind::logical_not, text, 0.0, start});
            else tokens.push_back(Token{TokenKind::identifier, text, 0.0, start});
            continue;
        }
        if (std::isdigit(ch) || (ch == '.' && at + 1 < source.size() &&
                                 std::isdigit(static_cast<unsigned char>(source[at + 1])))) {
            const auto start = at++;
            while (at < source.size() && std::isdigit(static_cast<unsigned char>(source[at]))) ++at;
            if (at < source.size() && source[at] == '.') {
                ++at;
                while (at < source.size() && std::isdigit(static_cast<unsigned char>(source[at]))) ++at;
            }
            if (at < source.size() && (source[at] == 'e' || source[at] == 'E')) {
                ++at;
                if (at < source.size() && (source[at] == '+' || source[at] == '-')) ++at;
                while (at < source.size() && std::isdigit(static_cast<unsigned char>(source[at]))) ++at;
            }
            const auto text = std::string(source.substr(start, at - start));
            try { tokens.push_back(Token{TokenKind::number, text, std::stod(text), start}); }
            catch (...) { throw Error("invalid formula number at byte " + std::to_string(start)); }
            continue;
        }
        const auto start = at++;
        switch (ch) {
        case '+': push(TokenKind::plus, start, 1); break;
        case '-': push(TokenKind::minus, start, 1); break;
        case '*': push(TokenKind::star, start, 1); break;
        case '/': push(TokenKind::slash, start, 1); break;
        case '%': push(TokenKind::percent, start, 1); break;
        case '(': push(TokenKind::left_paren, start, 1); break;
        case ')': push(TokenKind::right_paren, start, 1); break;
        case ',': push(TokenKind::comma, start, 1); break;
        case ';': push(TokenKind::semicolon, start, 1); break;
        case ':':
            if (at < source.size() && source[at] == '=') { ++at; push(TokenKind::assign, start, 2); }
            else push(TokenKind::colon, start, 1);
            break;
        case '=':
            if (at < source.size() && source[at] == '=') { ++at; push(TokenKind::equal, start, 2); }
            else push(TokenKind::equal, start, 1);
            break;
        case '<':
            if (at < source.size() && source[at] == '=') { ++at; push(TokenKind::less_equal, start, 2); }
            else if (at < source.size() && source[at] == '>') { ++at; push(TokenKind::not_equal, start, 2); }
            else push(TokenKind::less, start, 1);
            break;
        case '>':
            if (at < source.size() && source[at] == '=') { ++at; push(TokenKind::greater_equal, start, 2); }
            else push(TokenKind::greater, start, 1);
            break;
        case '!':
            if (at < source.size() && source[at] == '=') { ++at; push(TokenKind::not_equal, start, 2); }
            else push(TokenKind::logical_not, start, 1);
            break;
        case '&':
            if (at < source.size() && source[at] == '&') { ++at; push(TokenKind::logical_and, start, 2); }
            else throw Error("single '&' is not supported at byte " + std::to_string(start));
            break;
        case '|':
            if (at < source.size() && source[at] == '|') { ++at; push(TokenKind::logical_or, start, 2); }
            else throw Error("single '|' is not supported at byte " + std::to_string(start));
            break;
        default:
            throw Error("unsupported formula character at byte " + std::to_string(start));
        }
    }
    tokens.push_back(Token{TokenKind::end, {}, 0.0, source.size()});
    return tokens;
}

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    Program parse() {
        Program result;
        std::size_t unnamed = 0;
        while (!is(TokenKind::end)) {
            if (match(TokenKind::semicolon)) continue;
            std::string name;
            bool output = true;
            if (is(TokenKind::identifier) &&
                (peek(1).kind == TokenKind::assign || peek(1).kind == TokenKind::colon)) {
                name = upper_ascii(take().text);
                output = take().kind == TokenKind::colon;
                result.assigned.insert(name);
                if (output) result.outputs.push_back(name);
            } else {
                name = unnamed++ == 0 ? "RESULT" : "RESULT" + std::to_string(unnamed);
                result.outputs.push_back(name);
            }
            auto expression = parse_or(result);
            std::vector<std::string> directives;
            if (match(TokenKind::comma)) {
                // Everything after a completed expression at statement scope is a
                // drawing directive (COLOR*, LINETHICK*, NODRAW, ...). Numeric
                // calculation deliberately ignores these presentation attributes.
                while (!is(TokenKind::semicolon) && !is(TokenKind::end)) {
                    if (is(TokenKind::identifier)) {
                        auto directive = upper_ascii(take().text);
                        result.directives.insert(directive);
                        directives.push_back(std::move(directive));
                    }
                    else take();
                }
            }
            if (!match(TokenKind::semicolon) && !is(TokenKind::end))
                fail("expected ';' after formula statement");
            result.statements.push_back(
                Statement{name, output, std::move(expression), std::move(directives)});
        }
        return result;
    }

private:
    const Token& peek(std::size_t distance = 0) const {
        return tokens_[std::min(at_ + distance, tokens_.size() - 1)];
    }
    bool is(TokenKind kind) const { return peek().kind == kind; }
    Token take() { return tokens_[at_++]; }
    bool match(TokenKind kind) { if (!is(kind)) return false; ++at_; return true; }
    [[noreturn]] void fail(const std::string& message) const {
        throw Error(message + " at byte " + std::to_string(peek().offset));
    }
    std::unique_ptr<Node> unary(std::string op, std::unique_ptr<Node> value) {
        auto node = std::make_unique<Node>(); node->kind = NodeKind::unary; node->text = std::move(op);
        node->children.push_back(std::move(value)); return node;
    }
    std::unique_ptr<Node> binary(std::string op, std::unique_ptr<Node> left,
                                 std::unique_ptr<Node> right) {
        auto node = std::make_unique<Node>(); node->kind = NodeKind::binary; node->text = std::move(op);
        node->children.push_back(std::move(left)); node->children.push_back(std::move(right)); return node;
    }
    std::unique_ptr<Node> parse_or(Program& p) {
        auto value = parse_and(p);
        while (match(TokenKind::logical_or)) value = binary("OR", std::move(value), parse_and(p));
        return value;
    }
    std::unique_ptr<Node> parse_and(Program& p) {
        auto value = parse_equality(p);
        while (match(TokenKind::logical_and)) value = binary("AND", std::move(value), parse_equality(p));
        return value;
    }
    std::unique_ptr<Node> parse_equality(Program& p) {
        auto value = parse_relational(p);
        while (is(TokenKind::equal) || is(TokenKind::not_equal)) {
            const auto op = take().kind == TokenKind::equal ? "=" : "<>";
            value = binary(op, std::move(value), parse_relational(p));
        }
        return value;
    }
    std::unique_ptr<Node> parse_relational(Program& p) {
        auto value = parse_add(p);
        while (is(TokenKind::less) || is(TokenKind::less_equal) ||
               is(TokenKind::greater) || is(TokenKind::greater_equal)) {
            const auto kind = take().kind;
            const std::string op = kind == TokenKind::less ? "<" : kind == TokenKind::less_equal ? "<=" :
                                   kind == TokenKind::greater ? ">" : ">=";
            value = binary(op, std::move(value), parse_add(p));
        }
        return value;
    }
    std::unique_ptr<Node> parse_add(Program& p) {
        auto value = parse_multiply(p);
        while (is(TokenKind::plus) || is(TokenKind::minus)) {
            const auto op = take().kind == TokenKind::plus ? "+" : "-";
            value = binary(op, std::move(value), parse_multiply(p));
        }
        return value;
    }
    std::unique_ptr<Node> parse_multiply(Program& p) {
        auto value = parse_unary(p);
        while (is(TokenKind::star) || is(TokenKind::slash) || is(TokenKind::percent)) {
            const auto kind = take().kind;
            const std::string op = kind == TokenKind::star ? "*" : kind == TokenKind::slash ? "/" : "%";
            value = binary(op, std::move(value), parse_unary(p));
        }
        return value;
    }
    std::unique_ptr<Node> parse_unary(Program& p) {
        if (match(TokenKind::plus)) return unary("+", parse_unary(p));
        if (match(TokenKind::minus)) return unary("-", parse_unary(p));
        if (match(TokenKind::logical_not)) return unary("NOT", parse_unary(p));
        return parse_primary(p);
    }
    std::unique_ptr<Node> parse_primary(Program& p) {
        if (is(TokenKind::number)) {
            auto node = std::make_unique<Node>(); node->kind = NodeKind::number; node->number = take().number;
            return node;
        }
        if (is(TokenKind::string_literal)) {
            auto node = std::make_unique<Node>(); node->kind = NodeKind::string_literal;
            node->text = take().text;
            return node;
        }
        if (is(TokenKind::identifier)) {
            const auto name = upper_ascii(take().text);
            if (match(TokenKind::left_paren)) {
                auto node = std::make_unique<Node>(); node->kind = NodeKind::call; node->text = name;
                p.functions.insert(name);
                if (!match(TokenKind::right_paren)) {
                    do { node->children.push_back(parse_or(p)); } while (match(TokenKind::comma));
                    if (!match(TokenKind::right_paren)) fail("expected ')' in function call");
                }
                return node;
            }
            auto node = std::make_unique<Node>(); node->kind = NodeKind::symbol; node->text = name;
            p.symbols.insert(name); return node;
        }
        if (match(TokenKind::left_paren)) {
            auto value = parse_or(p);
            if (!match(TokenKind::right_paren)) fail("expected ')'");
            return value;
        }
        fail("expected formula expression");
    }

    std::vector<Token> tokens_;
    std::size_t at_{};
};

Program parse_formula_program(std::string_view source) {
    return Parser(lex(source)).parse();
}

namespace {

bool formula_reference_binding(std::string_view value) {
    constexpr std::string_view prefix = "EXTERNAL#";
    if (value.rfind(prefix, 0) != 0 ||
        value.find('$', prefix.size()) != std::string_view::npos ||
        value.find('#', prefix.size()) != std::string_view::npos)
        return false;
    const auto reference = value.substr(prefix.size());
    const auto separator = reference.rfind('.');
    return separator != std::string_view::npos && separator > 0 &&
           separator + 1 < reference.size();
}

std::string scalar_expression(const Node& node,
                              const std::set<std::string>& parameters,
                              bool& supported) {
    if (node.kind == NodeKind::number) {
        if (!std::isfinite(node.number)) {
            supported = false;
            return {};
        }
        std::ostringstream out;
        out << std::setprecision(std::numeric_limits<double>::max_digits10)
            << node.number;
        return out.str();
    }
    if (node.kind == NodeKind::symbol) {
        if (!parameters.count(node.text)) supported = false;
        return node.text;
    }
    if (node.kind == NodeKind::unary &&
        (node.text == "+" || node.text == "-") &&
        node.children.size() == 1) {
        return "(" + node.text +
               scalar_expression(*node.children[0], parameters, supported) +
               ")";
    }
    if (node.kind == NodeKind::binary && node.children.size() == 2 &&
        (node.text == "+" || node.text == "-" || node.text == "*" ||
         node.text == "/" || node.text == "%")) {
        return "(" + scalar_expression(*node.children[0], parameters, supported) +
               node.text +
               scalar_expression(*node.children[1], parameters, supported) +
               ")";
    }
    supported = false;
    return {};
}

double scalar_value(const Node& node,
                    const std::map<std::string, double>& parameters) {
    if (node.kind == NodeKind::number) return node.number;
    if (node.kind == NodeKind::symbol) {
        const auto found = parameters.find(node.text);
        if (found == parameters.end())
            throw Error("formula reference argument has an unbound parent parameter: " +
                        node.text);
        return found->second;
    }
    if (node.kind == NodeKind::unary && node.children.size() == 1 &&
        (node.text == "+" || node.text == "-")) {
        const double value = scalar_value(*node.children[0], parameters);
        return node.text == "-" ? -value : value;
    }
    if (node.kind == NodeKind::binary && node.children.size() == 2) {
        const double left = scalar_value(*node.children[0], parameters);
        const double right = scalar_value(*node.children[1], parameters);
        if (node.text == "+") return left + right;
        if (node.text == "-") return left - right;
        if (node.text == "*") return left * right;
        if (node.text == "/") {
            if (std::abs(right) <= 1e-15)
                throw Error("formula reference argument divides by zero");
            return left / right;
        }
        if (node.text == "%") {
            if (std::abs(right) <= 1e-15)
                throw Error("formula reference argument divides by zero");
            return std::fmod(left, right);
        }
    }
    throw Error("formula reference argument is not a scalar arithmetic expression");
}

}  // namespace

void collect_formula_reference_uses(
    const Node& node, const std::set<std::string>& parameters,
    std::vector<FormulaReferenceUse>& uses) {
    if ((node.kind == NodeKind::symbol || node.kind == NodeKind::call) &&
        formula_reference_binding(node.text)) {
        FormulaReferenceUse use;
        use.binding = node.text;
        use.scalar_arguments = true;
        if (node.kind == NodeKind::call) {
            for (const auto& child : node.children) {
                bool supported = true;
                auto expression = scalar_expression(
                    *child, parameters, supported);
                use.scalar_arguments = use.scalar_arguments && supported;
                use.argument_expressions.push_back(std::move(expression));
            }
        }
        uses.push_back(std::move(use));
    }
    for (const auto& child : node.children)
        collect_formula_reference_uses(*child, parameters, uses);
}

double evaluate_formula_scalar_expression(
    std::string_view expression,
    const std::map<std::string, double>& parameters) {
    auto program = parse_formula_program("X:" + std::string(expression) + ";");
    if (program.statements.size() != 1 ||
        !program.statements.front().expression)
        throw Error("invalid scalar formula reference argument");
    const double result = scalar_value(
        *program.statements.front().expression, parameters);
    if (!std::isfinite(result))
        throw Error("formula reference argument is not finite");
    return result;
}

void collect_numbered_context_calls(const Node& node, std::set<std::string>& required,
                                    std::set<std::string>& unavailable) {
    if (node.kind == NodeKind::call && node.text == "MAINZSHQ") {
        bool constant_arguments = node.children.size() == 2;
        std::array<int, 2> values{};
        for (std::size_t index = 0; constant_arguments && index < 2; ++index) {
            const auto& child = node.children[index];
            if (child->kind != NodeKind::number || !std::isfinite(child->number)) {
                constant_arguments = false;
                break;
            }
            const float narrowed = static_cast<float>(child->number);
            if (!std::isfinite(narrowed) || narrowed >= 2147483648.0f ||
                narrowed < -2147483648.0f) {
                constant_arguments = false;
                break;
            }
            values[index] = static_cast<int>(std::trunc(narrowed));
        }
        required.insert(constant_arguments
            ? "MAINZSHQ#" + std::to_string(values[0]) + "#" +
                  std::to_string(values[1])
            : "MAINZSHQ#DYNAMIC");
    }
    if (node.kind == NodeKind::call && node.text == "TOTALHQINFO") {
        bool constant_argument = node.children.size() == 1 &&
            node.children[0]->kind == NodeKind::number &&
            std::isfinite(node.children[0]->number);
        int value = 0;
        if (constant_argument) {
            const float narrowed = static_cast<float>(node.children[0]->number);
            constant_argument = std::isfinite(narrowed) &&
                narrowed < 2147483648.0f && narrowed >= -2147483648.0f;
            if (constant_argument) value = static_cast<int>(std::trunc(narrowed));
        }
        required.insert(constant_argument
            ? "TOTALHQINFO#" + std::to_string(value)
            : "TOTALHQINFO#DYNAMIC");
    }
    if (node.kind == NodeKind::call && node.text == "TOTALMMPAMO") {
        bool constant_argument = node.children.size() == 1 &&
            node.children[0]->kind == NodeKind::number &&
            std::isfinite(node.children[0]->number);
        int value = 0;
        if (constant_argument) {
            const float narrowed = static_cast<float>(node.children[0]->number);
            constant_argument = std::isfinite(narrowed) &&
                narrowed < 2147483648.0f && narrowed >= -2147483648.0f;
            if (constant_argument) value = static_cast<int>(std::trunc(narrowed));
        }
        if (!constant_argument) {
            required.insert("TOTALMMPAMO#DYNAMIC");
        } else if (value >= 1 && value <= 4) {
            required.insert("TOTALMMPAMO#" + std::to_string(value));
        } else if (value == 5 || value == 6) {
            unavailable.insert("TOTALMMPAMO#" + std::to_string(value) +
                               "#LEVEL2");
        }
    }
    if (node.kind == NodeKind::call &&
        (node.text == "EXTDATA_USER" || node.text == "SIGNALS_SYS" ||
         node.text == "SIGNALS_USER")) {
        bool constant_arguments = node.children.size() == 2;
        std::array<int, 2> values{};
        for (std::size_t index = 0; constant_arguments && index < 2; ++index) {
            const auto& child = node.children[index];
            if (child->kind != NodeKind::number || !std::isfinite(child->number)) {
                constant_arguments = false;
                break;
            }
            const float narrowed = static_cast<float>(child->number);
            if (!std::isfinite(narrowed) || narrowed >= 2147483648.0f ||
                narrowed < -2147483648.0f) {
                constant_arguments = false;
                break;
            }
            values[index] = static_cast<int>(std::trunc(narrowed));
        }
        if (constant_arguments)
            required.insert(node.text + "#" + std::to_string(values[0]) + "#" +
                            std::to_string(values[1]));
        else
            unavailable.insert(node.text + "#DYNAMIC");
    }
    if (node.kind == NodeKind::call && node.text == "BLOCKSETNUM") {
        if (node.children.size() == 1 &&
            node.children[0]->kind == NodeKind::string_literal) {
            required.insert("BLOCKSETNUM#" + node.children[0]->text);
        } else {
            unavailable.insert("BLOCKSETNUM#DYNAMIC");
        }
    }
    if (node.kind == NodeKind::call && node.text == "HORCALC") {
        bool constant_arguments = node.children.size() == 4 &&
            node.children[0]->kind == NodeKind::string_literal;
        std::array<int, 3> values{};
        for (std::size_t index = 1; constant_arguments && index < 4; ++index) {
            const auto& child = node.children[index];
            if (child->kind != NodeKind::number ||
                !std::isfinite(child->number) ||
                child->number != std::trunc(child->number)) {
                constant_arguments = false;
                break;
            }
            values[index - 1] = static_cast<int>(child->number);
        }
        constant_arguments = constant_arguments &&
            values[0] >= 100 && values[0] <= 106 &&
            values[1] >= 0 && values[1] <= 2 &&
            values[2] >= 0 && values[2] <= 4;
        if (constant_arguments) {
            required.insert("HORCALC#" + node.children[0]->text + "#" +
                            std::to_string(values[0]) + "#" +
                            std::to_string(values[1]) + "#" +
                            std::to_string(values[2]));
        } else {
            unavailable.insert("HORCALC#DYNAMIC");
        }
    }
    if (node.kind == NodeKind::call &&
        (node.text == "INSORT" || node.text == "INSUM")) {
        bool constant_arguments = node.children.size() == 4 &&
            node.children[0]->kind == NodeKind::string_literal &&
            node.children[1]->kind == NodeKind::string_literal;
        std::array<int, 2> values{};
        for (std::size_t index = 2; constant_arguments && index < 4; ++index) {
            const auto& child = node.children[index];
            if (child->kind != NodeKind::number ||
                !std::isfinite(child->number) ||
                child->number != std::trunc(child->number)) {
                constant_arguments = false;
                break;
            }
            values[index - 2] = static_cast<int>(child->number);
        }
        constant_arguments = constant_arguments && !node.children[0]->text.empty() &&
            !node.children[1]->text.empty() && values[0] >= 1 &&
            (node.text == "INSORT"
                 ? values[1] >= 0 && values[1] <= 1
                 : values[1] >= 0 && values[1] <= 5);
        if (constant_arguments) {
            required.insert(node.text + "#" + node.children[0]->text + "#" +
                            node.children[1]->text + "#" +
                            std::to_string(values[0]) + "#" +
                            std::to_string(values[1]));
        } else {
            unavailable.insert(node.text + "#DYNAMIC");
        }
    }
    if (node.kind == NodeKind::call && node.text == "CALCSTOCKINDEX") {
        bool constant_arguments = node.children.size() == 3 &&
            node.children[0]->kind == NodeKind::string_literal &&
            node.children[1]->kind == NodeKind::string_literal &&
            !node.children[0]->text.empty() &&
            !node.children[1]->text.empty() &&
            node.children[0]->text.find('#') == std::string::npos &&
            node.children[1]->text.find('#') == std::string::npos;
        int output = 0;
        if (constant_arguments) {
            const auto& child = node.children[2];
            constant_arguments = child->kind == NodeKind::number &&
                std::isfinite(child->number) &&
                child->number == std::trunc(child->number) &&
                child->number >= 1.0 && child->number <= 64.0;
            if (constant_arguments) output = static_cast<int>(child->number);
        }
        if (constant_arguments)
            required.insert("CALCSTOCKINDEX#" + node.children[0]->text +
                            "#" + node.children[1]->text + "#" +
                            std::to_string(output));
        else
            unavailable.insert("CALCSTOCKINDEX#DYNAMIC");
    }
    if (node.kind == NodeKind::call &&
        (node.text == "FINANCE" || node.text == "FINVALUE" || node.text == "DYNAINFO" ||
         node.text == "GPJYVALUE" || node.text == "BKJYVALUE" || node.text == "SCJYVALUE" ||
         node.text == "FINONE" || node.text == "GPJYONE" || node.text == "BKJYONE" ||
         node.text == "SCJYONE" || node.text == "GPONEDAT" ||
         node.text == "SPLIT" || node.text == "SPLITBARS" ||
         node.text == "SIGNALS_QS" || node.text == "L2_AMO" ||
         node.text == "L2_VOL" || node.text == "L2_VOLNUM")) {
        const std::size_t expected = node.text == "GPJYVALUE" || node.text == "BKJYVALUE" ||
                                     node.text == "SCJYVALUE" ? 3 :
                                     node.text == "GPJYONE" || node.text == "BKJYONE" ||
                                     node.text == "SCJYONE" ? 4 :
                                     node.text == "FINONE" ? 3 :
                                     node.text == "SPLIT" || node.text == "SPLITBARS" ||
                                     node.text == "SIGNALS_QS" ||
                                     node.text == "L2_AMO" ||
                                     node.text == "L2_VOL" ||
                                     node.text == "L2_VOLNUM" ? 2 : 1;
        bool constant_arguments = node.children.size() == expected;
        std::string binding = node.text;
        for (const auto& child : node.children) {
            if (child->kind != NodeKind::number || !std::isfinite(child->number)) {
                constant_arguments = false; break;
            }
            binding += "#" + std::to_string(static_cast<int>(child->number));
        }
        if (constant_arguments &&
            (node.text == "L2_AMO" || node.text == "L2_VOL" ||
             node.text == "L2_VOLNUM")) {
            const int first = static_cast<int>(node.children[0]->number);
            const int second = static_cast<int>(node.children[1]->number);
            const int maximum = node.text == "L2_VOLNUM" ? 1 : 3;
            constant_arguments = first >= 0 && first <= maximum &&
                                 second >= 0 && second <= maximum;
        }
        if (constant_arguments) required.insert(std::move(binding));
        else unavailable.insert(node.text + "#DYNAMIC");
    }
    for (const auto& child : node.children)
        collect_numbered_context_calls(*child, required, unavailable);
}


}  // namespace tdx::formula_language_detail

