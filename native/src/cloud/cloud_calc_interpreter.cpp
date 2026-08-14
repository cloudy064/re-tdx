#include "cloud_calc_interpreter_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::cloud_calc_detail {

class Expression {
public:
    using Lookup = std::function<double(std::string_view)>;

    Expression(std::string_view source, Lookup lookup, bool syntax_only = false)
        : source_(source), lookup_(std::move(lookup)), syntax_only_(syntax_only) {}

    double parse() {
        const double value = expression();
        whitespace();
        if (cursor_ != source_.size()) fail("unexpected token");
        return value;
    }

private:
    std::string_view source_;
    Lookup lookup_;
    bool syntax_only_{};
    std::size_t cursor_{};

    [[noreturn]] void fail(std::string_view message) const {
        throw Error("expression " + std::string(message) + " at byte " +
                    std::to_string(cursor_) + ": " + std::string(source_));
    }

    void whitespace() {
        while (cursor_ < source_.size() &&
               std::isspace(static_cast<unsigned char>(source_[cursor_]))) ++cursor_;
    }

    bool take(char value) {
        whitespace();
        if (cursor_ < source_.size() && source_[cursor_] == value) {
            ++cursor_;
            return true;
        }
        return false;
    }

    double expression() {
        double value = term();
        while (true) {
            if (take('+')) value += term();
            else if (take('-')) value -= term();
            else break;
        }
        return value;
    }

    double term() {
        double value = unary();
        while (true) {
            if (take('*')) value *= unary();
            else if (take('/')) {
                const double divisor = unary();
                if (divisor == 0.0 && !syntax_only_) fail("division by zero");
                value = divisor == 0.0 ? 1.0 : value / divisor;
            } else break;
        }
        return value;
    }

    double unary() {
        if (take('+')) return unary();
        if (take('-')) return -unary();
        return primary();
    }

    static bool identifier_char(char ch) {
        return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '$';
    }

    double primary() {
        whitespace();
        if (take('(')) {
            const double value = expression();
            if (!take(')')) fail("is missing ')'");
            return value;
        }
        if (cursor_ >= source_.size()) fail("ended unexpectedly");
        const char* begin = source_.data() + cursor_;
        char* end = nullptr;
        const double numeric = std::strtod(begin, &end);
        if (end != begin) {
            cursor_ += static_cast<std::size_t>(end - begin);
            return numeric;
        }
        if (!identifier_char(source_[cursor_])) fail("contains an unsupported character");
        const auto begin_index = cursor_;
        while (cursor_ < source_.size() && identifier_char(source_[cursor_])) ++cursor_;
        const auto name = source_.substr(begin_index, cursor_ - begin_index);
        whitespace();
        if (take('(')) {
            if (lower_ascii(std::string(name)) != "abs") fail("uses an unsupported function");
            const double value = expression();
            if (!take(')')) fail("is missing ')' after ABS");
            return std::fabs(value);
        }
        return lookup_(name);
    }
};

std::set<std::string, std::less<>> expression_identifiers(std::string_view source) {
    std::set<std::string, std::less<>> result;
    std::size_t cursor = 0;
    while (cursor < source.size()) {
        if (std::isdigit(static_cast<unsigned char>(source[cursor])) || source[cursor] == '.') {
            const char* begin = source.data() + cursor;
            char* end = nullptr;
            (void)std::strtod(begin, &end);
            if (end != begin) { cursor += static_cast<std::size_t>(end - begin); continue; }
        }
        if (!(std::isalnum(static_cast<unsigned char>(source[cursor])) ||
              source[cursor] == '_' || source[cursor] == '$')) {
            ++cursor;
            continue;
        }
        const auto begin = cursor;
        while (cursor < source.size() &&
               (std::isalnum(static_cast<unsigned char>(source[cursor])) ||
                source[cursor] == '_' || source[cursor] == '$')) ++cursor;
        auto name = std::string(source.substr(begin, cursor - begin));
        auto following = cursor;
        while (following < source.size() &&
               std::isspace(static_cast<unsigned char>(source[following]))) ++following;
        if (lower_ascii(name) == "abs" && following < source.size() && source[following] == '(')
            continue;
        result.insert(std::move(name));
    }
    return result;
}

FormulaCheck check_formula(const Column& column) {
    FormulaCheck result;
    if (const auto* builtin = find_builtin(column.calc)) {
        result.kind = "builtin";
        result.builtin = builtin;
        if (static_cast<int>(column.refs.size()) != builtin->argc) {
            result.reason = "calcref count " + std::to_string(column.refs.size()) +
                            " does not match registered argc " + std::to_string(builtin->argc);
            return result;
        }
        result.valid = true;
        result.executable = builtin->implemented;
        if (!result.executable) result.reason = "registered handler is not yet natively recovered";
        return result;
    }
    result.kind = "expression";
    try {
        Expression parser(column.calc, [](std::string_view) { return 1.0; }, true);
        (void)parser.parse();
        std::set<std::string, std::less<>> declared;
        for (const auto& ref : column.refs) {
            double literal = 0.0;
            if (!parse_double(ref, literal)) declared.insert(ref);
        }
        for (const auto& identifier : expression_identifiers(column.calc)) {
            if (!declared.count(identifier)) {
                result.reason = "expression identifier is absent from calcref: " + identifier;
                return result;
            }
        }
        result.valid = true;
        result.executable = true;
    } catch (const std::exception& error) {
        result.reason = error.what();
    }
    return result;
}

std::vector<std::string> dependency_cycles(const Unit& unit) {
    std::map<std::string, const Column*, std::less<>> derived;
    for (const auto& column : unit.columns)
        if (!column.calc.empty()) derived.emplace(column.code, &column);
    std::map<std::string, int, std::less<>> state;
    std::vector<std::string> stack;
    std::vector<std::string> cycles;
    std::function<void(const Column*)> visit = [&](const Column* column) {
        const int current = state[column->code];
        if (current == 2) return;
        if (current == 1) {
            const auto begin = std::find(stack.begin(), stack.end(), column->code);
            std::string cycle;
            for (auto item = begin; item != stack.end(); ++item) {
                if (!cycle.empty()) cycle += " -> ";
                cycle += *item;
            }
            if (!cycle.empty()) cycle += " -> ";
            cycle += column->code;
            cycles.push_back(std::move(cycle));
            return;
        }
        state[column->code] = 1;
        stack.push_back(column->code);
        for (const auto& ref : column->refs) {
            const auto found = derived.find(ref);
            if (found != derived.end()) visit(found->second);
        }
        stack.pop_back();
        state[column->code] = 2;
    };
    for (const auto& [code, column] : derived) {
        (void)code;
        visit(column);
    }
    std::sort(cycles.begin(), cycles.end());
    cycles.erase(std::unique(cycles.begin(), cycles.end()), cycles.end());
    return cycles;
}

using Environment = std::map<std::string, Json, std::less<>>;

Json reference_value(const Environment& environment, std::string_view reference) {
    double literal = 0.0;
    if (parse_double(reference, literal)) return literal;
    const auto found = environment.find(std::string(reference));
    if (found == environment.end() || found->second.is_null())
        throw Error("missing input " + std::string(reference));
    return found->second;
}

struct Order {
    std::vector<const Column*> columns;
    std::vector<std::string> cycles;
};

Order evaluation_order(const Unit& unit) {
    std::map<std::string, const Column*, std::less<>> derived;
    for (const auto& column : unit.columns)
        if (!column.calc.empty()) derived.emplace(column.code, &column);
    std::map<std::string, int, std::less<>> state;
    Order result;
    std::vector<std::string> stack;
    std::function<void(const Column*)> visit = [&](const Column* column) {
        const int current = state[column->code];
        if (current == 2) return;
        if (current == 1) {
            auto found = std::find(stack.begin(), stack.end(), column->code);
            std::string cycle;
            for (; found != stack.end(); ++found) {
                if (!cycle.empty()) cycle += " -> ";
                cycle += *found;
            }
            if (!cycle.empty()) cycle += " -> ";
            cycle += column->code;
            result.cycles.push_back(std::move(cycle));
            return;
        }
        state[column->code] = 1;
        stack.push_back(column->code);
        for (const auto& ref : column->refs) {
            const auto dependency = derived.find(ref);
            if (dependency != derived.end()) visit(dependency->second);
        }
        stack.pop_back();
        state[column->code] = 2;
        result.columns.push_back(column);
    };
    for (const auto& column : unit.columns)
        if (!column.calc.empty()) visit(&column);
    result.cycles = dependency_cycles(unit);
    return result;
}

Json evaluate_unit(const Unit& unit, const Json& row, int as_of) {
    Environment environment;
    for (const auto& [key, value] : row.as_object()) environment[key] = value;
    const auto order = evaluation_order(unit);
    Json results = Json::array();
    std::uint64_t evaluated = 0;
    std::uint64_t unavailable = 0;
    std::uint64_t errors = 0;
    std::set<std::string, std::less<>> cycle_codes;
    for (const auto& cycle : order.cycles) {
        const auto delimited = " -> " + cycle + " -> ";
        for (const auto& column : unit.columns)
            if (!column.calc.empty() &&
                delimited.find(" -> " + column.code + " -> ") != std::string::npos)
                cycle_codes.insert(column.code);
    }
    for (const auto* column : order.columns) {
        Json result = Json::object();
        result["code"] = column->code;
        if (!column->name.empty()) result["name"] = column->name;
        result["calc"] = column->calc;
        if (cycle_codes.count(column->code)) {
            result["status"] = "error";
            result["reason"] = "derived-column dependency cycle";
            ++errors;
            results.push_back(std::move(result));
            continue;
        }
        try {
            Json value;
            if (const auto* builtin = find_builtin(column->calc)) {
                std::vector<Json> arguments;
                arguments.reserve(column->refs.size());
                for (const auto& ref : column->refs) {
                    try {
                        arguments.push_back(reference_value(environment, ref));
                    } catch (const std::exception&) {
                        if (column->calcflag == 1) arguments.emplace_back(0.0);
                        else throw;
                    }
                }
                value = execute_builtin(*builtin, arguments, as_of);
            } else {
                Expression expression(column->calc, [&](std::string_view name) {
                    try {
                        return json_number(reference_value(environment, name), name);
                    } catch (const std::exception&) {
                        if (column->calcflag == 1) return 0.0;
                        throw;
                    }
                });
                value = expression.parse();
            }
            if (value.is_number() && !std::isfinite(value.as_number()))
                throw Error("calculation produced a non-finite number");
            environment[column->code] = value;
            result["status"] = "evaluated";
            result["value"] = value;
            ++evaluated;
        } catch (const std::exception& error) {
            const std::string reason = error.what();
            const bool missing = reason.rfind("missing input ", 0) == 0;
            result["status"] = missing ? "unavailable" : "error";
            result["reason"] = reason;
            if (missing) ++unavailable; else ++errors;
        }
        results.push_back(std::move(result));
    }
    Json report = Json::object();
    report["id"] = unit.id;
    report["resource"] = unit.file;
    Json counts = Json::object();
    counts["calculated"] = static_cast<std::uint64_t>(order.columns.size());
    counts["evaluated"] = evaluated;
    counts["unavailable"] = unavailable;
    counts["errors"] = errors;
    counts["cycles"] = static_cast<std::uint64_t>(order.cycles.size());
    report["counts"] = std::move(counts);
    Json cycles = Json::array();
    for (const auto& cycle : order.cycles) cycles.push_back(cycle);
    report["cycles"] = std::move(cycles);
    report["results"] = std::move(results);
    Json calculated = Json::object();
    for (const auto& [code, value] : environment)
        if (row.as_object().find(code) == row.as_object().end()) calculated[code] = value;
    report["calculated_values"] = std::move(calculated);
    return report;
}


}  // namespace tdx::cloud_calc_detail
