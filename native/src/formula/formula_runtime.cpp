#include "formula_runtime_internal.hpp"
#include "formula_native_constants.hpp"

#include "formula_engine_support_internal.hpp"
#include "formula_function_dispatch_internal.hpp"
#include "formula_operator_semantics_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::formula_runtime_detail {
using namespace formula_engine_detail;
using namespace formula_engine_support;
using namespace formula_language_detail;
using namespace formula_operator_semantics_detail;

namespace {

double tcalc_logical_and(double left, double right) {
    // TCalc!sub_10004CF0 gives exact zero priority over the missing sentinel.
    if (left == 0.0 || right == 0.0) return 0.0;
    if (!std::isfinite(left) || !std::isfinite(right)) return missing;
    return 1.0;
}

bool tcalc_logical_or_operand(double value) {
    if (!std::isfinite(value)) return false;
    constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    const double narrowed = native_float(value);
    return narrowed >= native_epsilon || narrowed <= -native_epsilon;
}

double tcalc_logical_or(double left, double right) {
    // TCalc!sub_10004780 treats the missing sentinel as false and applies the
    // native float epsilon independently to both operands.
    return tcalc_logical_or_operand(left) || tcalc_logical_or_operand(right)
        ? 1.0 : 0.0;
}

}  // namespace

Series evaluate_node(const Node& node, Environment& env,
                     const StringEnvironment& strings, std::size_t size);

bool evaluate_string_node(const Node& node, Environment& env,
                          const StringEnvironment& strings,
                          std::size_t size, StringSeries& output) {
    if (node.kind == NodeKind::string_literal) {
        output.assign(size, node.text);
        return true;
    }
    if (node.kind == NodeKind::symbol) {
        const auto found = strings.find(node.text);
        if (found != strings.end()) {
            output = found->second;
            return true;
        }
    }
    if (node.kind != NodeKind::call) return false;
    if (node.children.empty() &&
        custom_formula_security_relation_text_symbols.count(node.text)) {
        const auto found = strings.find(node.text);
        if (found == strings.end())
            throw Error(node.text + " requires automatic security-relation text context");
        output = found->second;
        return true;
    }
    if (node.text == "EXTERNSTR" && node.children.size() == 2) {
        if (!env.count("__EXTERN_SIGNALS_READY"))
            throw Error("EXTERNSTR requires automatic external-signal context");
        const auto namespace_value = evaluate_node(
            *node.children[0], env, strings, size).back();
        const auto external_id = evaluate_node(
            *node.children[1], env, strings, size).back();
        const auto key = "__EXTERNSTR#" +
                         tcalc_external_binding_key(namespace_value, external_id);
        const auto found = strings.find(key);
        // TdxW initializes offset +4 to a single space before searching.
        output.assign(size, found == strings.end() ? " " : found->second.back());
        return true;
    }
    if ((node.text == "GNBKZSCODE" || node.text == "FGBKZSCODE") &&
        node.children.size() == 1) {
        const auto ordinal = evaluate_node(
            *node.children[0], env, strings, size).back();
        std::string value;
        if (std::isfinite(ordinal) &&
            ordinal >= static_cast<double>(std::numeric_limits<int>::min()) &&
            ordinal <= static_cast<double>(std::numeric_limits<int>::max())) {
            const auto index = static_cast<int>(ordinal);
            const auto key = "__" + node.text + "#" + std::to_string(index);
            const auto found = strings.find(key);
            if (found != strings.end()) value = found->second.back();
        }
        output.assign(size, std::move(value));
        return true;
    }
    if (node.text == "GETNAMEOFCODE" && node.children.size() == 2) {
        const auto market_value = evaluate_node(
            *node.children[0], env, strings, size).back();
        StringSeries codes;
        if (!evaluate_string_node(
                *node.children[1], env, strings, size, codes))
            return false;
        int market = 0;
        if (std::isfinite(market_value) &&
            market_value >= static_cast<double>(std::numeric_limits<int>::min()) &&
            market_value <= static_cast<double>(std::numeric_limits<int>::max()))
            market = static_cast<std::uint16_t>(static_cast<int>(market_value));
        const auto key = std::make_pair(market, codes.back());
        std::string value;
        if (const auto found = strings.code_name_overrides.find(key);
            found != strings.code_name_overrides.end()) {
            value = found->second;
        } else if (strings.security_catalog) {
            const auto found = strings.security_catalog->find(key);
            if (found != strings.security_catalog->end()) value = found->second.name;
        }
        output.assign(size, std::move(value));
        return true;
    }
    if ((node.text == "STRCAT" || node.text == "VARCAT") &&
        node.children.size() == 2) {
        StringSeries left, right;
        if (!evaluate_string_node(*node.children[0], env, strings, size, left) ||
            !evaluate_string_node(*node.children[1], env, strings, size, right))
            return false;
        output.resize(size);
        if (node.text == "STRCAT") {
            const auto combined = left.back() + right.back();
            std::fill(output.begin(), output.end(), combined);
        } else {
            for (std::size_t i = 0; i < size; ++i)
                output[i] = left[i] + right[i];
        }
        return true;
    }
    if ((node.text == "STRCAT6" || node.text == "VARCAT6") &&
        node.children.size() == 6) {
        std::vector<StringSeries> values(6);
        for (std::size_t argument = 0; argument < values.size(); ++argument)
            if (!evaluate_string_node(*node.children[argument], env, strings,
                                      size, values[argument]))
                return false;
        output.resize(size);
        if (node.text == "STRCAT6") {
            std::string combined;
            for (const auto& value : values) combined += value.back();
            std::fill(output.begin(), output.end(), combined);
        } else {
            for (std::size_t i = 0; i < size; ++i)
                for (const auto& value : values) output[i] += value[i];
        }
        return true;
    }
    if ((node.text == "CON2STR" || node.text == "VAR2STR") &&
        node.children.size() == 2) {
        const auto values = evaluate_node(*node.children[0], env, strings, size);
        const auto decimals = evaluate_node(*node.children[1], env, strings, size);
        output.resize(size);
        if (node.text == "CON2STR") {
            const auto text = tcalc_number_text(values.back(), decimals.back());
            std::fill(output.begin(), output.end(), text);
        } else {
            for (std::size_t i = 0; i < size; ++i)
                output[i] = tcalc_number_text(values[i], decimals.back());
        }
        return true;
    }
    if (node.text == "STRSPACE" && node.children.size() == 1) {
        StringSeries value;
        if (!evaluate_string_node(*node.children[0], env, strings, size, value))
            return false;
        output.assign(size, value.back() + " ");
        return true;
    }
    if (node.text == "SUBSTR" && node.children.size() == 3) {
        StringSeries value;
        if (!evaluate_string_node(*node.children[0], env, strings, size, value))
            return false;
        const auto position = evaluate_node(*node.children[1], env, strings, size);
        const auto length = evaluate_node(*node.children[2], env, strings, size);
        output.assign(size, tcalc_substring(value.back(), position.back(),
                                            length.back()));
        return true;
    }
    if (node.text == "IF" && node.children.size() == 3) {
        StringSeries yes, no;
        if (!evaluate_string_node(*node.children[1], env, strings, size, yes) ||
            !evaluate_string_node(*node.children[2], env, strings, size, no))
            return false;
        const auto condition = evaluate_node(*node.children[0], env, strings, size);
        output.resize(size);
        bool condition_started = false;
        for (std::size_t i = 0; i < size; ++i) {
            if (!condition_started && !std::isfinite(condition[i])) continue;
            condition_started = true;
            output[i] = tcalc_if_select_true(condition[i]) ? yes[i] : no[i];
        }
        return true;
    }
    return false;
}

Series evaluate_node(const Node& node, Environment& env,
                     const StringEnvironment& strings, std::size_t size) {
    // TCalc compiled numeric nodes store one raw dword float.  Land literals
    // there before broadcasting so direct outputs and function arguments do
    // not retain parser-double precision unavailable to the native engine.
    if (node.kind == NodeKind::number)
        return constant(native_float(node.number), size);
    // String values are opaque handles in numeric expressions.  Dedicated
    // string functions inspect the AST and the exact UTF-8 binding instead.
    if (node.kind == NodeKind::string_literal) return constant(0.0, size);
    if (node.kind == NodeKind::symbol) {
        const auto found = env.find(node.text);
        if (found == env.end()) {
            // TCalc passes strings through opaque numeric handles.  Numeric
            // consumers receive only the placeholder while string-aware calls
            // and renderers recover the exact series from StringEnvironment.
            if (strings.count(node.text)) return constant(0.0, size);
            throw Error("unbound formula symbol: " + node.text);
        }
        return found->second;
    }
    if (node.kind == NodeKind::unary) {
        auto value = evaluate_node(*node.children[0], env, strings, size);
        if (node.text == "NOT") return tcalc_logical_not(value);
        for (auto& item : value) {
            if (node.text == "-") item = tcalc_negate(item);
        }
        return value;
    }
    if (node.kind == NodeKind::binary) {
        auto left = evaluate_node(*node.children[0], env, strings, size);
        const auto right = evaluate_node(*node.children[1], env, strings, size);
        for (std::size_t i = 0; i < size; ++i) {
            const double a = left[i], b = right[i];
            if (node.text == "AND") {
                left[i] = tcalc_logical_and(a, b); continue;
            }
            if (node.text == "OR") {
                left[i] = tcalc_logical_or(a, b); continue;
            }
            if (node.text == "/") {
                left[i] = tcalc_divide(a, b, i ? left[i - 1] : missing);
                continue;
            }
            if (node.text == "+") {
                left[i] = tcalc_add(a, b);
                continue;
            }
            if (node.text == "-") {
                left[i] = tcalc_subtract(a, b);
                continue;
            }
            if (node.text == "*") {
                left[i] = tcalc_multiply(a, b);
                continue;
            }
            if (node.text == "<") {
                left[i] = tcalc_compare(RelationalOperator::less, a, b);
                continue;
            }
            if (node.text == "<=") {
                left[i] = tcalc_compare(RelationalOperator::less_equal, a, b);
                continue;
            }
            if (node.text == ">") {
                left[i] = tcalc_compare(RelationalOperator::greater, a, b);
                continue;
            }
            if (node.text == ">=") {
                left[i] = tcalc_compare(RelationalOperator::greater_equal, a, b);
                continue;
            }
            if (node.text == "=") {
                left[i] = tcalc_equal(EqualityOperator::equal, a, b);
                continue;
            }
            if (node.text == "<>") {
                left[i] = tcalc_equal(EqualityOperator::not_equal, a, b);
                continue;
            }
            if (!std::isfinite(a) || !std::isfinite(b)) { left[i] = missing; continue; }
            if (node.text == "%") left[i] = std::abs(b) > 1e-15 ? std::fmod(a, b) : missing;
        }
        return left;
    }
    if (supported_formula_reference_dependency(node.text)) {
        std::vector<double> parameters;
        parameters.reserve(node.children.size());
        for (const auto& child : node.children) {
            const auto values = evaluate_node(*child, env, strings, size);
            if (values.empty() || !std::isfinite(values.front()))
                throw Error("formula reference parameter must be a finite scalar");
            const double scalar = values.front();
            if (std::any_of(values.begin(), values.end(),
                            [&](double value) {
                                return !std::isfinite(value) || value != scalar;
                            }))
                throw Error("formula reference parameters cannot vary by K-line bar");
            parameters.push_back(scalar);
        }
        const auto key = parameters.empty()
            ? node.text
            : parameterized_formula_reference_binding(node.text, parameters);
        const auto found = env.find(key);
        if (found == env.end())
            throw Error("formula output reference requires automatic library context: " +
                        key);
        return found->second;
    }
    if (node.text == "STRLEN") {
        if (node.children.size() != 1) throw Error("STRLEN expects 1 argument");
        StringSeries value;
        if (!evaluate_string_node(*node.children[0], env, strings, size, value))
            throw Error("STRLEN requires an evaluable string expression");
        return constant(
            static_cast<double>(tcalc_string_byte_length(value.back())), size);
    }
    if (node.text == "STRCMP") {
        if (node.children.size() != 2) throw Error("STRCMP expects 2 arguments");
        StringSeries left, right;
        if (!evaluate_string_node(*node.children[0], env, strings, size, left) ||
            !evaluate_string_node(*node.children[1], env, strings, size, right))
            throw Error("STRCMP requires an evaluable string expression");
        // TCalc!sub_1000F200 resolves the two opaque string handles from the
        // final argument slots, compares them once, and broadcasts the result.
        return constant(left.back() == right.back() ? 1.0 : 0.0, size);
    }
    if (node.text == "INBLOCK") {
        if (node.children.size() != 1) throw Error("INBLOCK expects 1 argument");
        StringSeries name;
        if (!evaluate_string_node(*node.children[0], env, strings, size, name))
            throw Error("INBLOCK requires an evaluable block-name string");
        const auto found = strings.find("__INBLOCK_MEMBERSHIPS");
        if (found == strings.end())
            throw Error("INBLOCK requires automatic block membership context");
        const std::string needle = "\x1F" + name.back() + "\x1F";
        return constant(found->second.back().find(needle) != std::string::npos
                            ? 1.0 : 0.0,
                        size);
    }
    if (node.text == "BLOCKSETNUM") {
        if (node.children.size() != 1)
            throw Error("BLOCKSETNUM expects 1 argument");
        StringSeries name;
        if (!evaluate_string_node(*node.children[0], env, strings, size, name))
            throw Error("BLOCKSETNUM requires an evaluable block-name string");
        const auto key = "BLOCKSETNUM#" + upper_ascii(name.back());
        const auto found = env.find(key);
        if (found == env.end())
            throw Error("BLOCKSETNUM requires automatic block catalog context");
        return found->second;
    }
    if (node.text == "HORCALC") {
        if (node.children.size() != 4)
            throw Error("HORCALC expects 4 arguments");
        StringSeries name;
        if (!evaluate_string_node(*node.children[0], env, strings, size, name))
            throw Error("HORCALC requires an evaluable block-name string");
        std::array<int, 3> values{};
        for (std::size_t index = 1; index < 4; ++index) {
            const auto series = evaluate_node(
                *node.children[index], env, strings, size);
            const double value = series.back();
            if (!std::isfinite(value) || value != std::trunc(value))
                throw Error("HORCALC numeric arguments must be integers");
            values[index - 1] = static_cast<int>(value);
        }
        if (values[0] < 100 || values[0] > 106 ||
            values[1] < 0 || values[1] > 2 ||
            values[2] < 0 || values[2] > 4)
            throw Error("HORCALC requires item 100..106, calculation 0..2, and weight 0..4");
        const auto key = upper_ascii(
            "HORCALC#" + name.back() + "#" + std::to_string(values[0]) +
            "#" + std::to_string(values[1]) + "#" +
            std::to_string(values[2]));
        const auto found = env.find(key);
        if (found == env.end())
            throw Error("HORCALC requires automatic horizontal block-series context");
        return found->second;
    }
    if (node.text == "INSORT" || node.text == "INSUM") {
        if (node.children.size() != 4)
            throw Error(node.text + " expects 4 arguments");
        StringSeries block, formula;
        if (!evaluate_string_node(*node.children[0], env, strings, size, block) ||
            !evaluate_string_node(*node.children[1], env, strings, size, formula))
            throw Error(node.text +
                        " requires evaluable block and indicator names");
        std::array<int, 2> values{};
        for (std::size_t index = 2; index < 4; ++index) {
            const auto series = evaluate_node(
                *node.children[index], env, strings, size);
            const double value = series.back();
            if (!std::isfinite(value) || value != std::trunc(value))
                throw Error(node.text + " numeric arguments must be integers");
            values[index - 2] = static_cast<int>(value);
        }
        if (values[0] < 1 ||
            (node.text == "INSORT"
                 ? values[1] < 0 || values[1] > 1
                 : values[1] < 0 || values[1] > 5))
            throw Error(node.text +
                        (node.text == "INSORT"
                             ? " requires output >= 1 and order 0..1"
                             : " requires output >= 1 and calculation 0..5"));
        const auto key = upper_ascii(
            node.text + "#" + block.back() + "#" + formula.back() + "#" +
            std::to_string(values[0]) + "#" + std::to_string(values[1]));
        const auto found = env.find(key);
        if (found == env.end())
            throw Error(node.text +
                        " requires automatic nested-indicator block context");
        return found->second;
    }
    if (node.text == "CALCSTOCKINDEX") {
        if (node.children.size() != 3)
            throw Error("CALCSTOCKINDEX expects 3 arguments");
        StringSeries security, formula;
        if (!evaluate_string_node(*node.children[0], env, strings, size,
                                  security) ||
            !evaluate_string_node(*node.children[1], env, strings, size,
                                  formula))
            throw Error("CALCSTOCKINDEX requires evaluable security and indicator names");
        const auto selected = evaluate_node(
            *node.children[2], env, strings, size);
        const double output = selected.back();
        if (!std::isfinite(output) || output != std::trunc(output) ||
            output < 1.0 || output > 64.0)
            throw Error("CALCSTOCKINDEX output must be an integer in 1..64");
        const auto key = upper_ascii(
            "CALCSTOCKINDEX#" + security.back() + "#" + formula.back() +
            "#" + std::to_string(static_cast<int>(output)));
        const auto found = env.find(key);
        if (found == env.end())
            throw Error("CALCSTOCKINDEX requires automatic nested-indicator context");
        return found->second;
    }
    if (node.text == "STR2CON" || node.text == "FINDSTR" ||
        node.text == "NAMELIKE" || node.text == "CODELIKE" ||
        node.text == "NAMEINCLUDE") {
        const std::size_t expected = node.text == "FINDSTR" ? 2 : 1;
        if (node.children.size() != expected)
            throw Error(node.text + " expects " + std::to_string(expected) +
                        " argument" + (expected == 1 ? "" : "s"));
        StringSeries first;
        if (!evaluate_string_node(*node.children[0], env, strings, size, first))
            throw Error(node.text + " requires an evaluable string expression");
        if (node.text == "STR2CON")
            return constant(native_float(std::atof(first.back().c_str())), size);
        if (node.text == "FINDSTR") {
            StringSeries second;
            if (!evaluate_string_node(*node.children[1], env, strings, size, second))
                throw Error("FINDSTR requires evaluable string expressions");
            return constant(first.back().find(second.back()) != std::string::npos
                                ? 1.0 : 0.0,
                            size);
        }
        const auto found = strings.find(
            node.text == "CODELIKE" ? "CODE" : "STKNAME");
        if (found == strings.end())
            throw Error(node.text + " requires security metadata");
        const auto& value = found->second.back();
        const auto& pattern = first.back();
        const bool match = node.text == "NAMEINCLUDE"
            ? value.find(pattern) != std::string::npos
            : value.rfind(pattern, 0) == 0;
        return constant(match ? 1.0 : 0.0, size);
    }
    if (node.children.size() == 1 &&
        node.children[0]->kind == NodeKind::number) {
        if (auto literal = evaluate_literal_scalar_builtin_function(
                node.text, node.children[0]->number, size))
            return std::move(*literal);
    }
    std::vector<Series> args; args.reserve(node.children.size());
    for (const auto& child : node.children)
        args.push_back(evaluate_node(*child, env, strings, size));
    return evaluate_call(node.text, args, env, size);
}

double json_number(const Json& row, std::string_view key) {
    const auto* value = optional(row, key);
    if (!value || !value->is_number()) throw Error("K-line bar is missing numeric field " + std::string(key));
    return value->as_number();
}

double json_optional_number(const Json& row, std::string_view key) {
    const auto* value = optional(row, key);
    return value && value->is_number()
        ? value->as_number() : std::numeric_limits<double>::quiet_NaN();
}

std::string json_text(const Json& row, std::string_view key) {
    const auto* value = optional(row, key);
    if (!value || !value->is_string()) throw Error("K-line bar is missing string field " + std::string(key));
    return value->as_string();
}

std::vector<Bar> read_bars(const Json& document) {
    const auto* rows = optional(document, "bars");
    if (!rows || !rows->is_array()) throw Error("K-line document does not contain a bars array");
    const auto* index = optional(document, "index_mode");
    const bool index_mode = index && index->is_bool() && index->as_bool();
    const auto* expansion = optional(document, "expansion_market");
    const bool expansion_market = expansion && expansion->is_bool() && expansion->as_bool();
    const auto* period_value = optional(document, "period");
    const auto period = period_value && period_value->is_string()
        ? upper_ascii(period_value->as_string()) : std::string{};
    const bool time_chart = period == "TIME" || period == "TIMELINE";
    std::vector<Bar> bars; bars.reserve(rows->size());
    for (const auto& row : rows->as_array()) {
        const auto volume = json_number(row, "volume");
        Bar bar;
        bar.date = json_text(row, "date");
        bar.time = json_text(row, "time");
        bar.open = json_number(row, "open");
        bar.high = json_number(row, "high");
        bar.low = json_number(row, "low");
        bar.close = json_number(row, "close");
        bar.amount = json_optional_number(row, "amount");
        bar.volume = volume;
        const double native_volume = native_float(volume);
        bar.formula_volume = native_float(
            index_mode || expansion_market ? native_volume : native_volume / 100.0);
        bar.open_interest = json_optional_number(row, "open_interest");
        bar.hk_short_volume = json_optional_number(row, "hk_short_volume");
        bar.auxiliary_price = json_optional_number(row, "auxiliary_price");
        if (!std::isfinite(bar.auxiliary_price))
            bar.auxiliary_price = json_optional_number(row, "average_price");
        if (!std::isfinite(bar.auxiliary_price))
            bar.auxiliary_price = json_optional_number(row, "settlement_price");
        if (!std::isfinite(bar.auxiliary_price))
            bar.auxiliary_price = bar.hk_short_volume;
        bars.push_back(std::move(bar));
    }
    std::stable_sort(bars.begin(), bars.end(), [](const Bar& a, const Bar& b) {
        return std::tie(a.date, a.time) < std::tie(b.date, b.time);
    });
    bars.erase(std::unique(bars.begin(), bars.end(), [](const Bar& a, const Bar& b) {
        return a.date == b.date && a.time == b.time;
    }), bars.end());
    // Ordinary 7709 historical-minute frames carry amount/volume rather than
    // the host's precomputed time-chart average.  Reconstruct the cumulative
    // VWAP per trading date before filling the common auxiliary slot.
    if (time_chart && !expansion_market) {
        std::string date;
        double amount = 0.0, volume = 0.0;
        for (auto& bar : bars) {
            if (bar.date != date) {
                date = bar.date;
                amount = 0.0;
                volume = 0.0;
            }
            if (std::isfinite(bar.amount) && std::isfinite(bar.volume) &&
                bar.volume > 0.0) {
                amount += bar.amount;
                volume += bar.volume;
            }
            if (!std::isfinite(bar.auxiliary_price) && volume > 0.0)
                bar.auxiliary_price = amount / volume;
        }
    }
    for (auto& bar : bars)
        if (!std::isfinite(bar.auxiliary_price)) bar.auxiliary_price = 0.0;
    return bars;
}

}  // namespace tdx::formula_runtime_detail
