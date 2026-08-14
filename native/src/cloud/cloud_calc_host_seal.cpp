#include "cloud_calc_host_internal.hpp"

#include "cloud_calc_builtins_internal.hpp"
#include "tdx/common.hpp"
#include "tdx/corporate.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace tdx::cloud_calc_detail {

bool seal_syscol(std::string_view value) {
    return value == "$FCAMO" || value == "$FCB";
}

bool depth_syscol(std::string_view value) {
    return value == "$BP1" || value == "$BPV1" ||
           value == "$SP1" || value == "$SPV1" || seal_syscol(value);
}

bool speed_syscol(std::string_view value) { return value == "$ZS"; }

bool quote_syscol(std::string_view value) {
    return value == "$CLOSE" || value == "$OPEN" || value == "$MAX" ||
           value == "$MIN" || value == "$NOW" || value == "$NOW2" ||
           value == "$NOW3" ||
           value == "$NOWV" || value == "$QRSD" || value == "$ZAF" ||
           value == "$ZEF" || value == "$ZQJC" || value == "$ZCJJE" ||
           value == "$CJL" || value == "$INP" || value == "$OUTP" ||
           value == "$JJJZ" || seal_syscol(value) ||
           depth_syscol(value) || speed_syscol(value);
}

SealLevel seal_level(const Json& quote, std::string_view side, std::size_t index) {
    SealLevel result;
    const auto* levels = host_optional(quote, side);
    if (!levels || !levels->is_array() || index >= levels->as_array().size() ||
        !levels->as_array()[index].is_object()) return result;
    const auto& level = levels->as_array()[index];
    if (const auto price = quote_number(level, "price")) result.price = *price;
    if (const auto volume = quote_number(level, "volume_hand"); volume && *volume > 0.0)
        result.volume_hand = static_cast<std::uint64_t>(*volume);
    return result;
}

Json enrich_seal_snapshots(Json& snapshots, const BlockData& blocks,
                           const LimitRuleConfig& rules, const Json& special_limits,
                           int as_of) {
    const int effective_date = as_of == 0 ? local_yyyymmdd() : as_of;
    std::uint64_t enriched = 0, special_matches = 0, unavailable = 0;
    for (auto& quote : mutable_snapshot_records(snapshots)) {
        if (!quote.is_object()) continue;
        const auto* market_value = host_optional(quote, "market_id");
        const auto* code_value = host_optional(quote, "code");
        if (!market_value || !code_value) continue;
        const auto market = host_market(*market_value);
        const auto code = scalar_text(*code_value);
        if (!market || code.size() != 6) continue;
        const auto security = blocks.securities.find({*market, code});
        if (security == blocks.securities.end()) {
            ++unavailable;
            continue;
        }
        const auto previous = quote_number(quote, "pre_close_price");
        const auto last = quote_number(quote, "last_price");
        if (!previous || !last) {
            ++unavailable;
            continue;
        }
        std::uint64_t flags = 0;
        if (const auto value = quote_number(quote, "status_raw"); value && *value > 0.0)
            flags = static_cast<std::uint64_t>(*value);
        std::int64_t imbalance = 0;
        auto imbalance_value = quote_number(quote, "auction_imbalance_hand_raw");
        if (!imbalance_value) imbalance_value = quote_number(quote, "unknown_after_outer_raw");
        if (imbalance_value) imbalance = static_cast<std::int64_t>(*imbalance_value);
        std::optional<SpecialLimitPrice> special;
        if (special_limits.is_object()) {
            static constexpr std::array<std::string_view, 3> names{"sz", "sh", "bj"};
            special = find_special_limit_price(
                special_limits, std::string(names.at(static_cast<std::size_t>(*market))), code);
            if (special) ++special_matches;
        }
        const auto limits = calculate_limit_prices(
            *market, code, security->second.name, *previous, effective_date, flags, rules, special);
        SealOrderInput input;
        input.last_price = *last;
        if (const auto total = quote_number(quote, "total_hand"); total && *total > 0.0)
            input.total_hand = static_cast<std::uint64_t>(*total);
        input.trade_unit = security->second.trade_unit;
        input.quote_flags = flags;
        input.auction_imbalance_hand = imbalance;
        for (std::size_t index = 0; index < 2; ++index) {
            input.buys[index] = seal_level(quote, "buy_levels", index);
            input.sells[index] = seal_level(quote, "sell_levels", index);
        }
        const auto seal = calculate_seal_order(input, limits);
        quote["trade_unit"] = security->second.trade_unit > 0.0
            ? Json(security->second.trade_unit) : Json(nullptr);
        quote["limit_up_price"] = limits.available ? Json(limits.upper) : Json(nullptr);
        quote["limit_down_price"] = limits.available ? Json(limits.lower) : Json(nullptr);
        quote["limit_price_source"] = limits.source;
        quote["seal_available"] = seal.available;
        quote["seal_direction"] = seal.direction;
        quote["seal_mode"] = seal.mode;
        quote["seal_queue_hand"] = static_cast<std::uint64_t>(seal.queue_hand);
        quote["seal_denominator_hand"] = static_cast<std::uint64_t>(seal.denominator_hand);
        quote["seal_amount_yuan"] = seal.available && seal.amount_available
            ? Json(seal.amount_yuan) : Json(nullptr);
        quote["seal_ratio"] = seal.available && seal.ratio_available
            ? Json(seal.ratio) : Json(nullptr);
        if (seal.available) ++enriched;
        else ++unavailable;
    }
    Json result = Json::object();
    result["schema"] = "tdx-tbigdata-seal-enrichment-v1";
    result["source_contract"] = "TdxW sub_5AA080 plus sub_9B37A0";
    result["as_of"] = effective_date;
    result["rule_source"] = rules.source;
    result["enriched"] = enriched;
    result["special_limit_matches"] = special_matches;
    result["unavailable"] = unavailable;
    return result;
}

}  // namespace tdx::cloud_calc_detail
