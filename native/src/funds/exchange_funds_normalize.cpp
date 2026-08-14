#include "exchange_funds_internal.hpp"

#include "tdx/common.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>

namespace tdx {

using exchange_fund_detail::change_pct;
using exchange_fund_detail::integer_value;
using exchange_fund_detail::kind_label;
using exchange_fund_detail::number;
using exchange_fund_detail::number_value;
using exchange_fund_detail::price_range;
using exchange_fund_detail::reference_instrument;
using exchange_fund_detail::resource_kind;
using exchange_fund_detail::security_document;
using exchange_fund_detail::text_value;

Json normalize_exchange_fund_rows(
    const std::string& resource, const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("exchange-fund rows must be an array");
    const auto kind = resource_kind(resource);
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto source_market = integer_value(raw, "$SC");
        const auto code = text_value(raw, "$ZQDM");
        const auto security = security_document(
            source_market, code, text_value(raw, "ZQJC"), securities);
        if (security.is_null()) continue;

        Json item = Json::object();
        item["kind"] = kind;
        item["kind_label"] = kind_label(kind);
        item["security"] = security;
        item["source_market_id"] = source_market;
        item["source_resource"] = resource;
        item["raw"] = raw;
        item["current_quote"] = Json(nullptr);
        item["event_id"] = resource + ":" +
            security.at("security_id").as_string();

        if (kind == "etf-performance") {
            const auto close = number_value(raw, "price0");
            item["snapshot_date"] = text_value(raw, "date");
            item["close_price"] = number(raw, "price0");
            item["turnover_yuan"] = number(raw, "drcje");
            item["turnover_5d_yuan"] = number(raw, "cje");
            item["reference_close_5d"] = number(raw, "price6");
            item["reference_close_20d"] = number(raw, "price21");
            item["reference_close_60d"] = number(raw, "price61");
            item["reference_close_month"] = number(raw, "price1");
            item["reference_close_ytd"] = number(raw, "price11");
            item["change_5d_pct"] = change_pct(
                close, number_value(raw, "price6"));
            item["change_20d_pct"] = change_pct(
                close, number_value(raw, "price21"));
            item["change_60d_pct"] = change_pct(
                close, number_value(raw, "price61"));
            item["change_month_pct"] = change_pct(
                close, number_value(raw, "price1"));
            item["change_ytd_pct"] = change_pct(
                close, number_value(raw, "price11"));
        } else if (kind == "etf-share-ranking") {
            item["snapshot_date"] = text_value(raw, "JZRQ");
            item["reference_instrument"] = reference_instrument(raw);
            item["net_inflow_yuan"] = number(raw, "JLR");
            item["latest_shares"] = number(raw, "ZXFE");
            item["share_change"] = number(raw, "FEBH");
            item["weekly_share_change"] = number(raw, "ZFEBH");
            item["monthly_share_change"] = number(raw, "YFEBH");
            item["prior_week_scale_yuan"] = number(raw, "YZGM");
            item["prior_month_scale_yuan"] = number(raw, "YYGM");
            item["subscription_unit_10k_shares"] = number(raw, "ZXSSDW");
            const auto unit = number_value(raw, "ZXSSDW");
            item["subscription_unit_shares"] = unit
                ? Json(*unit * 10000.0) : Json(nullptr);
            item["latest_scale_yuan"] = Json(nullptr);
            item["daily_scale_change_yuan"] = Json(nullptr);
            item["weekly_scale_change_yuan"] = Json(nullptr);
            item["monthly_scale_change_yuan"] = Json(nullptr);
            item["iopv"] = Json(nullptr);
            item["premium_pct"] = Json(nullptr);
            item["dataset_variant"] = resource;
        } else if (kind == "etf-scale-flow" || kind == "commodity-etf") {
            item["snapshot_date"] = text_value(raw, "JZRQ");
            item["nav_per_unit"] = number(raw, "DWJZ");
            item["latest_shares"] = number(raw, "ZXFE");
            item["share_change"] = number(raw, "FEBH");
            item["weekly_share_change"] = number(raw, "ZFEBH");
            item["monthly_share_change"] = number(raw, "YFEBH");
            item["latest_scale_yuan"] = number(raw, "YZGM");
            item["prior_scale_yuan"] = number(raw, "YYGM");
            item["subscription_unit_shares"] = number(raw, "ZXSSDW");
            item["max_subscription_fee_pct"] = number(raw, "ZGSGF");
            item["max_redemption_fee_pct"] = number(raw, "ZGSHF");
            item["reference_instrument"] = reference_instrument(raw);
            item["reference_change_pct"] = number(raw, "ZSZF");
            item["dataset_variant"] = resource;
        } else if (kind == "cash-arbitrage") {
            const auto yield = number_value(raw, "QRNH");
            const auto buy_interest_days = number_value(raw, "MRSHJXR");
            item["snapshot_date"] = text_value(raw, "TR");
            item["trade_date"] = text_value(raw, "TR");
            item["next_trade_date"] = text_value(raw, "TYR");
            item["settlement_date"] = text_value(raw, "ZJDZ");
            item["capital_tieup_days"] = number(raw, "ZKTS");
            item["seven_day_annualized_pct"] = number(raw, "QRNH");
            item["monthly_average_seven_day_pct"] = number(raw, "YJQRNH");
            item["yearly_average_seven_day_pct"] = number(raw, "NJQRNH");
            item["share_change"] = number(raw, "FEBH");
            item["latest_shares"] = number(raw, "ZXFE");
            item["buy_redeem_interest_days"] = number(raw, "MRSHJXR");
            item["subscribe_sell_interest_days"] = number(raw, "SGMCJXR");
            item["theoretical_nav"] = yield && buy_interest_days
                ? Json(100.0 + *buy_interest_days * *yield / 365.0)
                : Json(nullptr);
            item["premium_pct"] = Json(nullptr);
            item["buy_redeem_annualized_pct"] = Json(nullptr);
            item["subscribe_sell_annualized_pct"] = Json(nullptr);
        } else if (kind == "cash-yield") {
            item["snapshot_date"] = "";
            item["interest_calculation_type"] = text_value(raw, "JXLX");
            item["per_10k_yield"] = number(raw, "WFSY");
            item["seven_day_annualized_pct"] = number(raw, "QRNH");
            item["monthly_average_seven_day_pct"] = number(raw, "YJQRNH");
            item["yearly_average_seven_day_pct"] = number(raw, "NJQRNH");
            item["latest_shares_100m"] = number(raw, "ZXFE");
        } else if (kind == "lof") {
            item["snapshot_date"] = text_value(raw, "JZRQ");
            item["current_shares"] = number(raw, "CNFE");
            item["new_shares"] = number(raw, "XZFE");
            item["nav_per_unit"] = number(raw, "JJJZ");
            item["equity_ratio_pct"] = number(raw, "GPZB");
            item["bond_ratio_pct"] = number(raw, "ZQZB");
            item["max_subscription_fee_pct"] = number(raw, "ZGSGF");
            item["max_redemption_fee_pct"] = number(raw, "ZGSHF");
            item["subscription_status"] = text_value(raw, "SSZT");
            item["reference_instrument"] = reference_instrument(raw);
            item["dataset_variant"] = resource;
        } else if (kind == "closed-fund") {
            item["snapshot_date"] = text_value(raw, "JZRQ");
            item["nav_per_unit"] = number(raw, "DWJZ");
            item["maturity_date"] = text_value(raw, "DQRQ");
            item["remaining_years"] = number(raw, "SYNX");
            item["aggregate_discount_premium"] = number(raw, "ZJZZ");
            item["aggregate_estimated_value"] = number(raw, "ZJGZ");
            item["discount_pct"] = number(raw, "JJC");
            item["equity_ratio_pct"] = number(raw, "GPZB");
            item["bond_ratio_pct"] = number(raw, "ZQZB");
            item["dataset_variant"] = resource;
        } else if (kind == "cash-management-calendar") {
            item["snapshot_date"] = "";
            item["holding_days"] = number(raw, "TS");
            item["fee_pct"] = number(raw, "SXF");
            item["capital_available_date"] = text_value(raw, "ZJKY");
            item["market_settlement_date"] = text_value(raw, "SCZJJS");
            item["capital_withdrawable_date"] = text_value(raw, "ZJKQ");
            item["calendar_days"] = number(raw, "SJTS");
            item["available_days"] = number(raw, "ZJKYTS");
            item["withdrawable_days"] = number(raw, "ZJKQTS");
            item["dataset_variant"] = resource;
        } else {
            const auto range = price_range(text_value(raw, "xjqj"));
            item["snapshot_date"] = kind == "reit-pipeline"
                ? text_value(raw, "gxsj") : text_value(raw, "xjsj");
            item["status"] = kind == "reit-issued"
                ? "已发行" : text_value(raw, "xmzt");
            Json dates = Json::object();
            dates["updated"] = text_value(raw, "gxsj");
            dates["inquiry"] = text_value(raw, "xjsj");
            dates["online_subscription_start"] = text_value(raw, "gzrgqsr");
            dates["online_subscription_end"] = text_value(raw, "gzrgjzr");
            dates["financial_report"] = text_value(raw, "jzrq");
            item["dates"] = std::move(dates);
            item["inquiry_price_range"] = text_value(raw, "xjqj");
            item["inquiry_price_low"] = range.first
                ? Json(*range.first) : Json(nullptr);
            item["inquiry_price_high"] = range.second
                ? Json(*range.second) : Json(nullptr);
            item["subscription_price"] = number(raw, "rgj");
            item["term"] = text_value(raw, "qx");
            item["offering_total_units"] = number(raw, "zgm");
            item["strategic_placement_units"] = number(raw, "zlps");
            item["original_owner_subscription_units"] = number(raw, "ysrrg");
            item["offline_offering_units"] = number(raw, "wxgm");
            item["online_offering_units"] = number(raw, "wsgm");
            item["project_net_profit_yuan"] = number(raw, "mgsy");
            item["project_net_assets_yuan"] = number(raw, "mgjzc");
            item["project_net_cash_flow_yuan"] = number(raw, "mgxjl");
            item["project_description"] = text_value(raw, "jj");
            item["live_to_subscription_price_pct"] = Json(nullptr);
        }
        result.push_back(std::move(item));
    }
    return result;
}

}  // namespace tdx
