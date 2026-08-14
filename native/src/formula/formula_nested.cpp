#include "formula_nested_internal.hpp"

#include "formula_catalog_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/formula_context.hpp"
#include "tdx/formula_engine.hpp"
#include "tdx/minute.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>
#include <string_view>

namespace tdx::formula_context_detail {
namespace {

constexpr int native_warmup_bar_count = 3000;
constexpr int maximum_kline_pages = 20;
constexpr int maximum_kline_page_size = 800;

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string text_or(const Json& object, std::string_view key,
                    std::string fallback = {}) {
    const auto* value = optional(object, key);
    return value && value->is_string() ? value->as_string()
                                       : std::move(fallback);
}

int integer_or(const Json& object, std::string_view key, int fallback) {
    const auto* value = optional(object, key);
    return value && value->is_number()
        ? static_cast<int>(value->as_number()) : fallback;
}

bool boolean_or(const Json& object, std::string_view key, bool fallback) {
    const auto* value = optional(object, key);
    return value && value->is_bool() ? value->as_bool() : fallback;
}

struct CalcStockIndexBinding {
    std::string name;
    std::string security;
    std::string formula;
    int output{};
};

CalcStockIndexBinding parse_binding(const std::string& value) {
    const auto parts = split(value, '#');
    if (parts.size() != 4 || parts[0] != "CALCSTOCKINDEX" ||
        parts[1].empty() || parts[2].empty())
        throw Error("invalid CALCSTOCKINDEX analyzer binding: " + value);
    int output = 0;
    try {
        std::size_t used = 0;
        output = std::stoi(parts[3], &used);
        if (used != parts[3].size()) output = 0;
    } catch (...) {
        output = 0;
    }
    if (output < 1)
        throw Error("CALCSTOCKINDEX output must be a one-based positive integer");
    return {value, parts[1], parts[2], output};
}

struct SecurityIdentity {
    std::string market;
    std::string code;
    std::string kind;
};

SecurityIdentity parse_security(std::string value) {
    value = trim(value);
    auto normalized = lower_ascii(value);
    std::string market;
    std::string code;
    const auto colon = normalized.find(':');
    if (colon != std::string::npos) {
        market = normalized.substr(0, colon);
        code = normalized.substr(colon + 1);
    } else if (normalized.size() == 8) {
        market = normalized.substr(0, 2);
        code = normalized.substr(2);
    } else {
        code = normalized;
    }
    if (market == "0") market = "sz";
    else if (market == "1") market = "sh";
    else if (market == "2") market = "bj";
    if (!market.empty() && market != "sz" && market != "sh" && market != "bj")
        throw Error("CALCSTOCKINDEX security market must be SH/SZ/BJ: " + value);
    if (code.size() != 6 ||
        !std::all_of(code.begin(), code.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        }))
        throw Error("CALCSTOCKINDEX security must be a six-digit code, optionally prefixed by SH/SZ/BJ: " +
                    value);
    if (market.empty()) {
        if (code == "999999" || code.front() == '6') market = "sh";
        else if (code.rfind("899", 0) == 0) market = "bj";
        else market = "sz";
    }
    const bool index =
        (market == "sh" && (code == "999999" || code.front() == '0')) ||
        (market == "sz" && code.rfind("399", 0) == 0) ||
        (market == "bj" && code.rfind("899", 0) == 0);
    return {market, code, index ? "index" : "auto"};
}

std::string stamp(const Json& row) {
    const auto date = text_or(row, "date");
    return date.empty() ? std::string{} : date + "|" + text_or(row, "time");
}

Json align_output(const Json& caller, const Json& evaluation,
                  const std::string& output_name,
                  std::uint64_t& exact_matches,
                  std::uint64_t& one_bar_carries) {
    const auto* caller_rows = optional(caller, "bars");
    const auto* nested_rows = optional(evaluation, "points");
    if (!caller_rows || !caller_rows->is_array() ||
        !nested_rows || !nested_rows->is_array())
        throw Error("CALCSTOCKINDEX requires caller bars and nested formula points");

    std::map<std::string, double, std::less<>> exact;
    for (const auto& row : nested_rows->as_array()) {
        const auto key = stamp(row);
        const auto* values = optional(row, "values");
        const auto* value = values ? optional(*values, output_name) : nullptr;
        if (!key.empty() && value && value->is_number() &&
            std::isfinite(value->as_number()))
            exact[key] = value->as_number();
    }

    Json result = Json::object();
    bool previous_was_exact = false;
    double previous_value = 0.0;
    for (const auto& row : caller_rows->as_array()) {
        const auto key = stamp(row);
        if (key.empty()) {
            previous_was_exact = false;
            continue;
        }
        const auto found = exact.find(key);
        if (found != exact.end()) {
            result[key] = found->second;
            previous_value = found->second;
            previous_was_exact = true;
            ++exact_matches;
        } else {
            if (previous_was_exact) {
                result[key] = previous_value;
                ++one_bar_carries;
            }
            // TCalc only bridges the bar immediately following a matched bar;
            // a carried value cannot seed another carry.
            previous_was_exact = false;
        }
    }
    return result;
}

}  // namespace

Json next_nested_formula_state(const Json* state, const std::string& identity,
                               std::string_view consumer) {
    int depth = 0;
    std::set<std::string> stack;
    Json stack_json = Json::array();
    if (state && state->is_object()) {
        depth = integer_or(*state, "depth", 0);
        if (const auto* values = optional(*state, "stack");
            values && values->is_array())
            for (const auto& value : values->as_array())
                if (value.is_string()) {
                    stack.insert(lower_ascii(value.as_string()));
                    stack_json.push_back(value);
                }
    }
    if (depth >= maximum_nested_formula_depth)
        throw Error(std::string(consumer) + " nested formula depth exceeds " +
                    std::to_string(maximum_nested_formula_depth));
    if (stack.count(lower_ascii(identity)))
        throw Error(std::string(consumer) +
                    " recursive indicator cycle detected at " + identity);
    stack_json.push_back(identity);
    Json result = Json::object();
    result["depth"] = depth + 1;
    result["stack"] = std::move(stack_json);
    return result;
}

void bind_calcstockindex_context(
    Json& context, const std::filesystem::path& root,
    const std::string& current_market, const std::string& current_code,
    const Json& caller_kline, const std::vector<std::string>& raw_bindings,
    const Json& formula_library, int timeout_ms, const BlockData* block_data,
    const std::filesystem::path& jsn_root, const Json* nested_state) {
    if (raw_bindings.empty()) return;
    if (!context.as_object().count("series")) context["series"] = Json::object();

    std::map<std::string, Json, std::less<>> kline_cache;
    std::map<std::string, Json, std::less<>> evaluation_cache;
    Json resolutions = Json::array();
    const auto period = text_or(caller_kline, "period", "day");
    const int caller_count = std::max(
        integer_or(caller_kline, "downloaded",
                   integer_or(caller_kline, "count", 1)), 1);
    const int caller_start = std::max(integer_or(caller_kline, "start", 0), 0);
    const int wanted = std::min(
        maximum_kline_pages * maximum_kline_page_size,
        caller_start + caller_count + native_warmup_bar_count);
    const int pages = std::max(
        1, (wanted + maximum_kline_page_size - 1) /
               maximum_kline_page_size);

    for (const auto& raw : raw_bindings) {
        const auto binding = parse_binding(raw);
        const auto security = parse_security(binding.security);
        const auto selection = select_technical_formula(
            formula_library, binding.formula, binding.output,
            FormulaSelectionPolicy::automatic_nested_context,
            "CALCSTOCKINDEX");
        const auto identity = security.market + ":" + security.code + ":" +
                              lower_ascii(selection.code);
        auto child_state = next_nested_formula_state(
            nested_state, identity, "CALCSTOCKINDEX");

        const auto security_key = security.market + ":" + security.code + ":" +
                                  security.kind + ":" + period + ":" +
                                  std::to_string(pages);
        auto kline = kline_cache.find(security_key);
        if (kline == kline_cache.end()) {
            auto document = fetch_kline_document(
                security.market, security.code, security.kind, period, pages,
                maximum_kline_page_size, 0, "all", timeout_ms, root);
            kline = kline_cache.emplace(security_key, std::move(document)).first;
        }

        const auto evaluation_key = security_key + ":" +
                                    lower_ascii(selection.code);
        auto evaluated = evaluation_cache.find(evaluation_key);
        if (evaluated == evaluation_cache.end()) {
            Json child_context = Json::object();
            const bool needs_context =
                boolean_or(selection.analysis, "has_external_dependency", false);
            if (needs_context) {
                child_context = build_formula_market_context_document(
                    root, security.market, security.code, selection.analysis,
                    timeout_ms, block_data, &kline->second, false,
                    &formula_library, jsn_root, &child_state);
                child_context["automatic_market_context"] = true;
            }
            auto document = evaluate_formula_document(
                kline->second, *selection.definition, {},
                child_context.size() ? &child_context : nullptr);
            evaluated = evaluation_cache.emplace(
                evaluation_key, std::move(document)).first;
        }

        std::uint64_t exact_matches = 0;
        std::uint64_t one_bar_carries = 0;
        context["series"][binding.name] = align_output(
            caller_kline, evaluated->second, selection.output_name,
            exact_matches, one_bar_carries);

        Json item = Json::object();
        item["binding"] = binding.name;
        item["caller_security"] =
            lower_ascii(current_market) + ":" + current_code;
        item["target_security"] = security.market + ":" + security.code;
        item["formula"] = selection.code;
        item["formula_name"] = selection.name;
        item["output_index"] = binding.output;
        item["output_name"] = selection.output_name;
        item["period"] = period;
        item["warmup_bars"] = native_warmup_bar_count;
        item["requested_pages"] = pages;
        item["nested_depth"] = integer_or(child_state, "depth", 1);
        item["target_bar_count"] = integer_or(evaluated->second, "count", 0);
        item["exact_match_count"] = exact_matches;
        item["one_bar_carry_count"] = one_bar_carries;
        resolutions.push_back(std::move(item));
    }

    context["calcstockindex_resolutions"] = std::move(resolutions);
    context["calcstockindex_binding_count"] =
        static_cast<std::uint64_t>(raw_bindings.size());
    context["calcstockindex_maximum_depth"] = maximum_nested_formula_depth;
    context["calcstockindex_warmup_bars"] = native_warmup_bar_count;
    context["calcstockindex_mode"] =
        "tcalc-opcode1109-same-period-catalog-output-date-time-aligned-one-gap-carry";
}

}  // namespace tdx::formula_context_detail
