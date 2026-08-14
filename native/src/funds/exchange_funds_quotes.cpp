#include "exchange_funds_internal.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <map>
#include <string>

namespace tdx {

using exchange_fund_detail::change_pct;
using exchange_fund_detail::digits;
using exchange_fund_detail::number_value;
using exchange_fund_detail::quote_for;
using exchange_fund_detail::text_value;

void apply_exchange_fund_quotes(Json& rows, const Json& quote_rows) {
    if (!rows.is_array() || !quote_rows.is_array())
        throw Error("exchange-fund records and quotes must be arrays");
    std::map<std::pair<int, std::string>, Json> quotes;
    for (const auto& quote : quote_rows.as_array()) {
        const auto market = number_value(quote, "market_id");
        const auto code = text_value(quote, "code");
        if (market && digits(code))
            quotes[{static_cast<int>(*market), code}] = quote;
    }
    for (auto& row : rows.as_array()) {
        const auto quote = quote_for(row.at("security"), quotes);
        row["current_quote"] = quote;
        if (quote.is_null()) continue;
        const auto current = number_value(quote, "last_price");
        const auto kind = text_value(row, "kind");
        if (kind == "etf-performance") {
            row["live_change_from_snapshot_close_pct"] = change_pct(
                current, number_value(row, "close_price"));
        } else if (kind == "etf-share-ranking") {
            const auto latest_shares = number_value(row, "latest_shares");
            const auto daily_shares = number_value(row, "share_change");
            const auto prior_week_scale = number_value(
                row, "prior_week_scale_yuan");
            const auto prior_month_scale = number_value(
                row, "prior_month_scale_yuan");
            const auto iopv = number_value(quote, "fund_iopv");
            row["iopv"] = iopv ? Json(*iopv) : Json(nullptr);
            row["latest_scale_yuan"] = current && latest_shares
                ? Json(*current * *latest_shares) : Json(nullptr);
            row["daily_scale_change_yuan"] = current && daily_shares
                ? Json(*current * *daily_shares) : Json(nullptr);
            row["weekly_scale_change_yuan"] =
                current && latest_shares && prior_week_scale
                    ? Json(*current * *latest_shares - *prior_week_scale)
                    : Json(nullptr);
            row["monthly_scale_change_yuan"] =
                current && latest_shares && prior_month_scale
                    ? Json(*current * *latest_shares - *prior_month_scale)
                    : Json(nullptr);
            row["premium_pct"] = change_pct(current, iopv);
        } else if (kind == "cash-arbitrage") {
            const auto theoretical = number_value(row, "theoretical_nav");
            const auto capital_days = number_value(row, "capital_tieup_days");
            const auto yield = number_value(row, "seven_day_annualized_pct");
            const auto sell_days = number_value(
                row, "subscribe_sell_interest_days");
            row["premium_pct"] = change_pct(current, theoretical);
            row["buy_redeem_annualized_pct"] =
                current && theoretical && capital_days &&
                std::abs(*current) > 0.000001 &&
                std::abs(*capital_days) > 0.000001
                    ? Json((*theoretical - *current) / *current * 365.0 /
                           *capital_days * 100.0)
                    : Json(nullptr);
            row["subscribe_sell_annualized_pct"] =
                current && yield && sell_days && capital_days &&
                std::abs(*capital_days) > 0.000001
                    ? Json(((*current - 100.0) + *yield * *sell_days / 365.0) /
                           100.0 * 365.0 / *capital_days * 100.0)
                    : Json(nullptr);
        } else if (kind == "reit-issued" || kind == "reit-pipeline") {
            row["live_to_subscription_price_pct"] = change_pct(
                current, number_value(row, "subscription_price"));
        }
    }
}

}  // namespace tdx
