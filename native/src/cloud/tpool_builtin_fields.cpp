#include "tpool_internal.hpp"

#include <array>
#include <cmath>
#include <ctime>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string_view>

namespace tdx::tpool_detail {
namespace {

enum Requirement : unsigned {
    finance_required = 1U << 0,
    quote_required = 1U << 1,
    directory_required = 1U << 2,
    trade_unit_required = 1U << 3,
};

struct BuiltinFieldDefinition {
    int set;
    int index;
    std::string_view key;
    std::string_view name;
    std::string_view unit;
    unsigned requirements;
};

// The order and labels are the two reversed, contiguous UI catalogs beside
// TPool.dll's comparison operator strings. The indices and projections are
// independently fixed by sub_1001B9E0's nset=3/4 switches.
constexpr std::array<BuiltinFieldDefinition, 42> kBuiltinFields{{
    {3, 0, "total_shares", "总股本", "ten-thousand-shares", finance_required},
    {3, 1, "circulating_shares", "流通股本", "ten-thousand-shares", finance_required},
    {3, 2, "b_shares", "B股/A股", "ten-thousand-shares", finance_required},
    {3, 3, "h_shares", "H股", "ten-thousand-shares", finance_required},
    {3, 4, "ab_market_value", "AB股总市值", "ten-thousand-yuan", finance_required | quote_required},
    {3, 5, "circulating_market_value", "流通市值", "ten-thousand-yuan", finance_required | quote_required},
    {3, 6, "total_assets", "总资产", "ten-thousand-yuan", finance_required},
    {3, 7, "current_assets", "流动资产", "ten-thousand-yuan", finance_required},
    {3, 8, "intangible_assets", "无形资产", "ten-thousand-yuan", finance_required},
    {3, 9, "current_liabilities", "流动负债", "ten-thousand-yuan", finance_required},
    {3, 10, "capital_reserve", "资本公积金", "ten-thousand-yuan", finance_required},
    {3, 11, "accounts_receivable", "应收账款", "ten-thousand-yuan", finance_required},
    {3, 12, "operating_profit", "营业利润", "ten-thousand-yuan", finance_required},
    {3, 13, "investment_income", "投资收益", "ten-thousand-yuan", finance_required},
    {3, 14, "operating_cash_flow", "经营现金流量", "ten-thousand-yuan", finance_required},
    {3, 15, "total_cash_flow", "总现金流量", "ten-thousand-yuan", finance_required},
    {3, 16, "inventory", "存货", "ten-thousand-yuan", finance_required},
    {3, 17, "net_profit", "净利润", "ten-thousand-yuan", finance_required},
    {3, 18, "undistributed_profit", "未分配利润", "ten-thousand-yuan", finance_required},
    {3, 19, "net_assets", "净资产(股东权益)", "ten-thousand-yuan", finance_required},
    {3, 20, "net_assets_per_share", "每股净资产", "yuan-per-share", finance_required},
    {3, 21, "capital_reserve_per_share", "每股公积金", "yuan-per-share", finance_required},
    {3, 22, "undistributed_profit_per_share", "每股未分配", "yuan-per-share", finance_required},
    {3, 23, "earnings_per_share", "每股收益", "yuan-per-share", finance_required},
    {3, 24, "shareholder_equity_ratio", "股东权益比", "percent", finance_required},
    {3, 25, "minority_interest_compat", "少数股权", "ten-thousand-yuan", finance_required},
    {3, 26, "operating_revenue", "营业收入", "ten-thousand-yuan", finance_required},
    {3, 27, "total_profit", "利润总额", "ten-thousand-yuan", finance_required},
    {3, 28, "after_tax_profit", "税后利润", "ten-thousand-yuan", finance_required},
    {3, 29, "return_on_equity", "净资产收益率", "percent", finance_required},
    {4, 0, "last_price", "现价", "yuan-per-share", quote_required},
    {4, 1, "high_price", "最高", "yuan-per-share", quote_required},
    {4, 2, "low_price", "最低", "yuan-per-share", quote_required},
    {4, 3, "open_price", "今开", "yuan-per-share", quote_required},
    {4, 4, "pre_close_price", "昨收", "yuan-per-share", quote_required},
    {4, 5, "total_volume", "总量", "hand", quote_required},
    {4, 6, "total_amount", "总金额", "yuan", quote_required},
    {4, 7, "change_pct", "涨幅", "percent", quote_required},
    {4, 8, "amplitude_pct", "振幅", "percent", quote_required},
    {4, 9, "dynamic_pe", "市盈(动)", "ratio", quote_required | finance_required},
    {4, 10, "turnover_pct", "换手率", "percent", quote_required | finance_required | trade_unit_required},
    {4, 11, "volume_ratio", "量比", "ratio", quote_required | directory_required},
}};

const BuiltinFieldDefinition* builtin_field(int set, int index) {
    for (const auto& field : kBuiltinFields)
        if (field.set == set && field.index == index) return &field;
    return nullptr;
}

const Json* nested(const Json* value,
                   std::initializer_list<std::string_view> path) {
    const Json* current = value;
    for (const auto part : path) {
        if (!current) return nullptr;
        current = optional(*current, part);
    }
    return current;
}

std::optional<double> number(const Json* value,
                             std::initializer_list<std::string_view> path) {
    const auto* found = nested(value, path);
    if (!found || !found->is_number() || !std::isfinite(found->as_number()))
        return std::nullopt;
    return found->as_number();
}

std::optional<double> scaled(const Json* value,
                             std::initializer_list<std::string_view> path,
                             double divisor) {
    const auto raw = number(value, path);
    return raw ? std::optional<double>(*raw / divisor) : std::nullopt;
}

std::optional<double> market_value_price(const Json* quote) {
    const auto last = number(quote, {"last_price"});
    if (last && std::abs(*last) >= 0.00001) return last;
    return number(quote, {"pre_close_price"});
}

std::optional<double> latest_finance_value(int index,
                                           const Json* quote,
                                           const Json* finance) {
    const auto total = number(finance, {"shares", "total"});
    const auto circulating = number(finance, {"shares", "circulating"});
    switch (index) {
        case 0: return total ? std::optional<double>(*total / 10000.0) : std::nullopt;
        case 1: return circulating ? std::optional<double>(*circulating / 10000.0) : std::nullopt;
        case 2: return scaled(finance, {"shares", "b_share"}, 10000.0);
        case 3: return scaled(finance, {"shares", "h_share"}, 10000.0);
        case 4: {
            const auto h = number(finance, {"shares", "h_share"});
            const auto price = market_value_price(quote);
            return total && h && price
                ? std::optional<double>((*total - *h) * *price / 10000.0)
                : std::nullopt;
        }
        case 5: {
            const auto price = market_value_price(quote);
            return circulating && price
                ? std::optional<double>(*circulating * *price / 10000.0)
                : std::nullopt;
        }
        case 6: return scaled(finance, {"balance_sheet", "total_assets_yuan"}, 10000.0);
        case 7: return scaled(finance, {"balance_sheet", "current_assets_yuan"}, 10000.0);
        case 8: return scaled(finance, {"balance_sheet", "intangible_assets_yuan"}, 10000.0);
        case 9: return scaled(finance, {"balance_sheet", "current_liabilities_yuan"}, 10000.0);
        case 10: return scaled(finance, {"balance_sheet", "capital_reserve_yuan"}, 10000.0);
        case 11: return scaled(finance, {"balance_sheet", "accounts_receivable_yuan"}, 10000.0);
        case 12: return scaled(finance, {"income_statement", "operating_profit_yuan"}, 10000.0);
        case 13: return scaled(finance, {"income_statement", "investment_income_yuan"}, 10000.0);
        case 14: return scaled(finance, {"cash_flow", "operating_yuan"}, 10000.0);
        case 15: return scaled(finance, {"cash_flow", "total_yuan"}, 10000.0);
        case 16: return scaled(finance, {"balance_sheet", "inventory_yuan"}, 10000.0);
        case 17: return scaled(finance, {"income_statement", "net_profit_yuan"}, 10000.0);
        case 18: return scaled(finance, {"income_statement", "undistributed_profit_yuan"}, 10000.0);
        case 19: return scaled(finance, {"balance_sheet", "net_assets_yuan"}, 10000.0);
        case 20: return number(finance, {"per_share", "net_assets"});
        case 21: {
            const auto reserve = number(finance, {"balance_sheet", "capital_reserve_yuan"});
            return total && reserve && std::abs(*total) >= 0.00001
                ? std::optional<double>(*reserve / *total) : std::nullopt;
        }
        case 22: {
            const auto profit = number(finance, {"income_statement", "undistributed_profit_yuan"});
            return total && profit && std::abs(*total) >= 0.00001
                ? std::optional<double>(*profit / *total) : std::nullopt;
        }
        case 23: return number(finance, {"per_share", "eps"});
        case 24: {
            const auto assets = number(finance, {"balance_sheet", "total_assets_yuan"});
            const auto equity = number(finance, {"balance_sheet", "net_assets_yuan"});
            return assets && equity && std::abs(*assets) >= 0.00001
                ? std::optional<double>(*equity / *assets * 100.0) : std::nullopt;
        }
        case 25:
            // This surprising alias is intentional: the DLL's UI label is
            // 少数股权, but sub_1001B9E0 reads callback offset +113, which is
            // the native 0x0010 long-term-liabilities slot.
            return scaled(finance, {"balance_sheet", "long_term_liabilities_yuan"}, 10000.0);
        case 26: return scaled(finance, {"income_statement", "revenue_yuan"}, 10000.0);
        case 27: return scaled(finance, {"income_statement", "total_profit_yuan"}, 10000.0);
        case 28: return scaled(finance, {"income_statement", "after_tax_profit_yuan"}, 10000.0);
        case 29: return number(finance, {"reserved_2"});
        default: return std::nullopt;
    }
}

std::optional<double> nonzero_quote(const Json* quote, std::string_view key) {
    const auto value = number(quote, {key});
    return value && std::abs(*value) >= 0.00001 ? value : std::nullopt;
}

std::optional<double> realtime_quote_value(int index,
                                           const Json* quote,
                                           const Json* finance,
                                           double volume_ratio_base,
                                           double trade_unit,
                                           int elapsed_trading_minutes) {
    switch (index) {
        case 0: return nonzero_quote(quote, "last_price");
        case 1: return nonzero_quote(quote, "high_price");
        case 2: return nonzero_quote(quote, "low_price");
        case 3: return nonzero_quote(quote, "open_price");
        case 4: return nonzero_quote(quote, "pre_close_price");
        case 5: return nonzero_quote(quote, "total_hand");
        case 6: return nonzero_quote(quote, "amount");
        case 7: {
            const auto last = nonzero_quote(quote, "last_price");
            const auto previous = nonzero_quote(quote, "pre_close_price");
            return last && previous
                ? std::optional<double>((*last - *previous) / *previous * 100.0)
                : std::nullopt;
        }
        case 8: {
            const auto high = nonzero_quote(quote, "high_price");
            const auto low = nonzero_quote(quote, "low_price");
            return high && low
                ? std::optional<double>((*high - *low) / *low * 100.0)
                : std::nullopt;
        }
        case 9: {
            const auto last = nonzero_quote(quote, "last_price");
            const auto total = number(finance, {"shares", "total"});
            const auto eps = number(finance, {"per_share", "eps"});
            return last && total && *total > 1.0 && eps && std::abs(*eps) > 0.00001
                ? std::optional<double>(*last / *eps) : std::nullopt;
        }
        case 10: {
            const auto hands = number(quote, {"total_hand"});
            const auto circulating = number(finance, {"shares", "circulating"});
            return hands && circulating && *circulating > 1.0 &&
                   std::isfinite(trade_unit) && trade_unit > 0.0
                ? std::optional<double>(*hands * trade_unit * 100.0 / *circulating)
                : std::nullopt;
        }
        case 11: {
            const auto hands = number(quote, {"total_hand"});
            return hands && std::isfinite(volume_ratio_base) &&
                   volume_ratio_base > 0.00001 && elapsed_trading_minutes > 0
                ? std::optional<double>(*hands / elapsed_trading_minutes /
                                        volume_ratio_base)
                : std::nullopt;
        }
        default: return std::nullopt;
    }
}

}  // namespace

RuleSourceKind tpool_rule_source_kind(int set) {
    if (set >= 0 && set <= 2) return RuleSourceKind::formula;
    if (set == 3) return RuleSourceKind::latest_finance;
    if (set == 4) return RuleSourceKind::realtime_quote;
    return RuleSourceKind::unknown;
}

std::string tpool_rule_source_name(RuleSourceKind kind) {
    switch (kind) {
        case RuleSourceKind::formula: return "formula";
        case RuleSourceKind::latest_finance: return "latest-finance";
        case RuleSourceKind::realtime_quote: return "realtime-quote";
        default: return "unknown";
    }
}

std::string tpool_operator_name(int set, int operation) {
    if (tpool_rule_source_kind(set) == RuleSourceKind::formula)
        return operator_name(operation);
    if (set != 3 && set != 4) return "unknown";
    switch (operation) {
        case 0: return "equal";
        case 1: return "greater-than";
        case 2: return "less-than";
        case 3: return "exact-rank";
        case 4: return "top-or-bottom-n";
        case 5: return "rank-tail";
        default: return "unknown";
    }
}

bool tpool_supported_operation(int set, int operation) {
    const auto kind = tpool_rule_source_kind(set);
    if (kind == RuleSourceKind::formula) return supported_operation(operation);
    if (kind == RuleSourceKind::latest_finance ||
        kind == RuleSourceKind::realtime_quote)
        return operation >= 0 && operation <= 5;
    return false;
}

bool tpool_ranking_operation(int set, int operation) {
    return tpool_rule_source_kind(set) == RuleSourceKind::formula
        ? ranking_operation(operation)
        : (set == 3 || set == 4) && operation >= 3 && operation <= 5;
}

int tpool_canonical_ranking_operation(int set, int operation) {
    if (!tpool_ranking_operation(set, operation)) return -1;
    return tpool_rule_source_kind(set) == RuleSourceKind::formula
        ? operation : operation + 2;
}

BuiltinRulePlan annotate_tpool_builtin_rule(Json& rule) {
    BuiltinRulePlan plan;
    const int set = json_integer_text(rule, "nset", -1);
    plan.kind = tpool_rule_source_kind(set);
    plan.applicable = plan.kind == RuleSourceKind::latest_finance ||
                      plan.kind == RuleSourceKind::realtime_quote;
    if (!plan.applicable) return plan;
    const int index = json_integer_text(rule, "ntjindexno", -1);
    const int operation = json_integer_text(rule, "noperate", -1);
    const auto* field = builtin_field(set, index);
    plan.field_recognized = field != nullptr;
    plan.operation_supported = tpool_supported_operation(set, operation);
    plan.execution_ready = plan.field_recognized && plan.operation_supported;
    if (!field)
        plan.blocking_reason = "unknown TPool built-in field index";
    else if (!plan.operation_supported)
        plan.blocking_reason = "unsupported built-in comparison operation";

    rule["rule_kind"] = tpool_rule_source_name(plan.kind);
    rule["field_index"] = index;
    rule["field_mapping_verified"] = field != nullptr;
    rule["operator"] = tpool_operator_name(set, operation);
    rule["operator_mapping_verified"] = plan.operation_supported;
    rule["calculation_engine"] = "tdx-tpool-builtin-field-v1";
    rule["formula_supported"] = nullptr;
    rule["period"] = "";
    rule["history_window_mode"] = "current-snapshot";
    rule["history_window_supported"] = true;
    rule["history_attributes_applied"] = false;
    rule["history_window_begin_offset"] = json_integer_text(rule, "nbeginday", 0);
    rule["history_window_end_offset"] = json_integer_text(rule, "nendday", 0);
    rule["history_minimum_bars"] = 0;
    rule["history_fetch_bars"] = 0;
    rule["calculation_lookback_bars"] = nullptr;
    rule["history_blocking_reason"] = "";
    rule["history_mapping_source"] =
        "TPool.dll sub_1001B9E0 direct nset=3/4 branch (current values only)";
    Json sources = Json::array();
    if (field) {
        rule["field_key"] = std::string(field->key);
        rule["field_name"] = std::string(field->name);
        rule["field_unit"] = std::string(field->unit);
        rule["field_mapping_source"] =
            "TPool.dll sub_1001B9E0 switch plus CP936 UI catalog at 0x56D6C..0x56F14";
        plan.needs_finance = (field->requirements & finance_required) != 0;
        plan.needs_quote = (field->requirements & quote_required) != 0;
        plan.needs_directory = (field->requirements & directory_required) != 0;
        plan.needs_trade_unit = (field->requirements & trade_unit_required) != 0;
        if (plan.needs_quote) sources.push_back("public-l1-snapshot-0x054c");
        if (plan.needs_finance) sources.push_back("latest-finance-0x0010");
        if (plan.needs_directory) sources.push_back("security-directory-volume-ratio-base");
        if (plan.needs_trade_unit) sources.push_back("local-tnf-trade-unit");
        if (set == 3 && index == 25)
            rule["compatibility_note"] =
                "DLL label is 少数股权 but its exact callback offset is the 0x0010 long-term-liabilities slot";
    } else {
        rule["field_key"] = "";
        rule["field_name"] = "";
        rule["field_unit"] = "";
    }
    rule["required_sources"] = std::move(sources);
    return plan;
}

RuleResult evaluate_tpool_builtin_rule(const Json& rule,
                                       const Json* quote,
                                       const Json* finance,
                                       double volume_ratio_base,
                                       double trade_unit,
                                       int elapsed_trading_minutes) {
    RuleResult result;
    const int set = json_integer_text(rule, "nset", -1);
    const int index = json_integer_text(rule, "ntjindexno", -1);
    const int operation = json_integer_text(rule, "noperate", -1);
    const auto* field = builtin_field(set, index);
    if (!field) {
        result.error = "unknown TPool built-in field index";
        return result;
    }
    if (!tpool_supported_operation(set, operation)) {
        result.error = "unsupported built-in comparison operation";
        return result;
    }
    const auto value = set == 3
        ? latest_finance_value(index, quote, finance)
        : realtime_quote_value(index, quote, finance, volume_ratio_base,
                               trade_unit, elapsed_trading_minutes);
    if (!value || !std::isfinite(*value)) {
        result.error = "required value for built-in field " +
                       std::string(field->key) + " is unavailable";
        return result;
    }
    result.left = *value;
    result.left_name = std::string(field->key);
    result.right = json_float_text(rule, "fsecond", 0.0);
    result.right_name = tpool_ranking_operation(set, operation)
        ? "rank-threshold" : "constant";
    if (!std::isfinite(result.right)) {
        result.error = "built-in comparison threshold is not finite";
        return result;
    }
    if (tpool_ranking_operation(set, operation)) {
        result.ranking_pending = true;
        return result;
    }
    constexpr double epsilon = 0.00001;
    result.matched = operation == 0
        ? std::abs(result.left - result.right) < epsilon
        : (operation == 1 ? result.left > result.right
                          : result.left < result.right);
    result.evaluated = true;
    return result;
}

int current_a_share_elapsed_trading_minutes() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now)) return 1;
#else
    if (!localtime_r(&now, &local)) return 1;
#endif
    const int minute = local.tm_hour * 60 + local.tm_min;
    // Outside a live weekday session the public quote is the completed prior
    // session. TPool's type-122 market-status callback takes the full-session
    // branch in that state; using one minute would inflate a stale 量比 240x.
    if (local.tm_wday == 0 || local.tm_wday == 6 || minute < 9 * 60 + 30)
        return 240;
    if (minute == 9 * 60 + 30) return 1;
    if (minute <= 11 * 60 + 30) return minute - (9 * 60 + 30);
    if (minute < 13 * 60) return 120;
    if (minute < 15 * 60) return 120 + minute - 13 * 60;
    return 240;
}

}  // namespace tdx::tpool_detail
