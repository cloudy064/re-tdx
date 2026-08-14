#include "cloud_calc_host_internal.hpp"

#include "cloud_calc_builtins_internal.hpp"
#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace tdx::cloud_calc_detail {

const Json* nested_value(const Json& value,
                         std::initializer_list<std::string_view> path) {
    const Json* current = &value;
    for (const auto key : path) {
        current = host_optional(*current, key);
        if (!current) return nullptr;
    }
    return current;
}

FinanceIndex index_finance(const Json& document) {
    FinanceIndex result;
    for (const auto& record : snapshot_records(document)) {
        if (!record.is_object()) continue;
        const auto* market_value = host_optional(record, "market_id");
        const auto* code_value = host_optional(record, "code");
        if (!market_value || !code_value) continue;
        const auto market = host_market(*market_value);
        const auto code = scalar_text(*code_value);
        if (market && code.size() == 6) result[{*market, code}] = &record;
    }
    return result;
}

std::optional<double> nested_number(const Json& value,
                                    std::initializer_list<std::string_view> path) {
    const auto* found = nested_value(value, path);
    if (!found) return std::nullopt;
    try { return json_number(*found, "finance field"); } catch (...) { return std::nullopt; }
}

std::optional<HostValue> finance_host_value(std::string_view syscol,
                                            const Json& finance,
                                            const Json* quote) {
    const auto circulating = nested_number(finance, {"shares", "circulating"});
    const auto total = nested_number(finance, {"shares", "total"});
    if (syscol == "$J_LTGB" && circulating && *circulating > 0.0)
        return HostValue{*circulating, "public-finance-0x0010-circulating-shares"};
    if (syscol == "$J_ZGB" && total && *total > 0.0)
        return HostValue{*total, "public-finance-0x0010-total-shares"};
    if (!quote) return std::nullopt;
    const auto price_value = quote_host_value("$NOW2", *quote);
    if (!price_value) return std::nullopt;
    const double price = price_value->value.as_number();
    if (syscol == "$PE") {
        const auto net_profit = nested_number(
            finance, {"income_statement", "net_profit_yuan"});
        const auto report_months = nested_number(finance, {"reserved_2"});
        if (net_profit && total && report_months && *total > 0.0 &&
            *report_months > 0.0) {
            // TdxW type-105 writes Destination+185 as the annualized EPS:
            // finance[59] * 12 / finance[62] / finance[33].  The public
            // 0x0010 decoder normalizes those fields to yuan, months and
            // shares, so the scale factors cancel here.  The native path
            // stores the intermediate in a float before DYNAINFO(39) divides
            // the current/pre-close fallback price by it.
            const float annualized_eps = static_cast<float>(
                *net_profit * 12.0 / *report_months / *total);
            if (annualized_eps > 0.00009999999747378752f)
                return HostValue{
                    static_cast<double>(static_cast<float>(
                        price / static_cast<double>(annualized_eps))),
                    "tdxw-dynainfo39-annualized-net-profit-plus-public-l1"};
        }
        return std::nullopt;
    }
    if (syscol == "$J_LTSZ" && circulating && *circulating > 0.0)
        return HostValue{price * *circulating,
                         "public-l1-price-times-0x0010-circulating-shares"};
    if (syscol == "$HSL" && circulating && *circulating > 0.0) {
        const auto total_hand = quote_number(*quote, "total_hand");
        if (total_hand)
            return HostValue{*total_hand * 10000.0 / *circulating,
                             "public-l1-total-hand-over-0x0010-circulating-shares"};
    }
    if (syscol == "$J_ZSZ" && total && *total > 0.0) {
        const auto b_share = nested_number(finance, {"shares", "b_share"});
        const auto h_share = nested_number(finance, {"shares", "h_share"});
        // TBigData ID 22 falls back to A-price * (total - H shares).  When B
        // shares exist its command-120 direct value may use a separate B quote,
        // which 0x054C for the selected A security does not contain.
        if (b_share && *b_share > 0.0) return std::nullopt;
        const double mainland = *total - (h_share ? *h_share : 0.0);
        if (mainland > 0.0)
            return HostValue{price * mainland,
                             "tbigdata-id22-a-price-times-total-minus-h-shares"};
    }
    return std::nullopt;
}

const Block* host_industry_block(const BlockData& data,
                                 const std::pair<int, std::string>& security) {
    for (const auto family : {"industry", "research-industry"}) {
        for (const auto& member : data.members) {
            if (member.market_id != security.first || member.code != security.second ||
                member.family != family || member.membership != "direct") continue;
            const auto block = std::find_if(data.blocks.begin(), data.blocks.end(),
                [&](const Block& value) { return value.block_id == member.block_id; });
            if (block != data.blocks.end()) return &*block;
        }
    }
    return nullptr;
}

std::optional<double> current_coupon_rate(const Json& row) {
    const auto* value = host_optional(row, "SYFXLLXL");
    if (!value) return std::nullopt;
    try {
        const auto rates = json_rates(*value, "remaining coupon rates");
        return rates.empty() ? std::nullopt : std::optional<double>(rates.front());
    } catch (...) { return std::nullopt; }
}

std::optional<double> bond_accrued_interest(const Json& row, int as_of) {
    const auto* face_value = host_optional(row, "MZ");
    const auto* previous_value = host_optional(row, "SGFXRQ");
    const auto* next_value = host_optional(row, "XGFXRQ");
    const auto rate = current_coupon_rate(row);
    if (!face_value || !previous_value || !next_value || !rate) return std::nullopt;
    try {
        const double face = json_number(*face_value, "bond face value");
        const int previous = json_date(*previous_value, "previous coupon date");
        const int next = json_date(*next_value, "next coupon date");
        const int current = as_of == 0 ? local_yyyymmdd() : as_of;
        const auto current_days = date_serial(current);
        const auto previous_days = date_serial(previous);
        const auto next_days = date_serial(next);
        if (current_days < previous_days || current_days > next_days) return std::nullopt;
        return face * *rate * static_cast<double>(current_days - previous_days) / 365.0;
    } catch (...) { return std::nullopt; }
}

}  // namespace tdx::cloud_calc_detail
