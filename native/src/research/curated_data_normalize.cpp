#include "tdx/curated_data.hpp"
#include "tdx/curated_data_internal.hpp"
#include "tdx/common.hpp"

#include <cmath>

namespace tdx {

using detail::curated_data::kind_for;
using detail::curated_data::label_for;
using detail::curated_data::source_market;
using detail::curated_data::security_document;
using detail::curated_data::text_value;
using detail::curated_data::number;
using detail::curated_data::number_value;
using detail::curated_data::scaled;

Json normalize_curated_data_rows(
    const std::string& resource, const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities,
    const std::map<std::string, std::string>& hong_kong_names) {
    if (!rows.is_array()) throw Error("curated-data rows must be an array");
    const auto kind = kind_for(resource);
    Json result = Json::array();
    std::size_t index = 0;
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["kind"] = kind;
        item["kind_label"] = label_for(kind);
        item["source_resource"] = resource;
        item["raw"] = row;
        Json security(nullptr);
        std::string date;

        if (kind != "buyback-statistics") {
            const auto code = text_value(row, "$ZQDM");
            security = security_document(source_market(row), code,
                                         securities, hong_kong_names);
            if (security.is_null()) continue;
        }

        if (kind == "media-entertainment") {
            date = text_value(row, "date");
            item["movie_count"] = number(row, "sldy");
            item["drama_count"] = number(row, "sldsj");
            item["variety_count"] = number(row, "slzy");
            item["release_date"] = date;
            item["title"] = text_value(row, "name");
            item["background"] = text_value(row, "bjzl");
            item["background_excerpt"] = detail::curated_data::excerpt(
                text_value(row, "bjzl"));
        } else if (kind == "low-valuation-smallcap") {
            date = text_value(row, "ZXRQ");
            item["rating_date"] = date;
            item["institution_count"] = number(row, "JGSL");
            item["composite_rating"] = number(row, "ZHPJ");
            item["pe"] = number(row, "PE");
            item["forecast_eps_growth_pct"] = number(row, "YCEPS");
            const auto pe = number_value(row, "PE");
            const auto growth = number_value(row, "YCEPS");
            item["estimated_peg"] = pe && growth && std::abs(*growth) > 1e-12
                ? Json(*pe / *growth) : Json(nullptr);
            item["market_cap_yuan"] = number(row, "SZ");
            item["pe_ttm"] = number(row, "PE_TTM");
            item["target_price"] = number(row, "MBJ");
            item["report_period"] = text_value(row, "bgq");
            item["forecast_profit_lower_yuan"] = number(row, "jlr1");
            item["forecast_profit_upper_yuan"] = number(row, "jlr2");
            item["forecast_profit_growth_pct_lower"] = number(row, "zj1");
            item["forecast_profit_growth_pct_upper"] = number(row, "zj2");
            item["reference_close"] = number(row, "closeprice");
        } else if (kind == "dividend-fundraising") {
            item["cumulative_dividend_100m_yuan"] = number(row, "ljfh");
            item["cumulative_dividend_yuan"] = scaled(row, "ljfh", 1e8);
            item["dividend_count"] = number(row, "ljfhcs");
            item["cumulative_fundraising_100m_yuan"] = number(row, "ljmj");
            item["cumulative_fundraising_yuan"] = scaled(row, "ljmj", 1e8);
            item["fundraising_count"] = number(row, "ljmjcs");
            const auto dividend = number_value(row, "ljfh");
            const auto fundraising = number_value(row, "ljmj");
            item["dividend_fundraising_ratio"] = dividend && fundraising &&
                std::abs(*fundraising) > 1e-12
                ? Json(*dividend / *fundraising) : Json(nullptr);
            item["dividend_yield_pct"] = number(row, "GXL");
        } else if (kind == "buyback-statistics") {
            date = text_value(row, "date");
            item["month"] = date;
            item["planned_buyback_10k_shares"] = number(row, "hgsl");
            item["planned_buyback_shares"] = scaled(row, "hgsl", 1e4);
            item["planned_buyback_100m_yuan"] = number(row, "hgsz");
            item["planned_buyback_yuan"] = scaled(row, "hgsz", 1e8);
            item["company_count"] = number(row, "hggps");
            item["annual_profit_yuan"] = number(row, "nlr");
            item["a_share_market_cap_yuan"] = number(row, "J_ZSZ");
            item["circulating_market_cap_yuan"] = number(row, "J_LTSZ");
            const auto amount = number_value(row, "hgsz");
            const auto total = number_value(row, "J_ZSZ");
            const auto circulating = number_value(row, "J_LTSZ");
            const auto profit = number_value(row, "nlr");
            const auto ratio = [&](const std::optional<double>& denominator) {
                return amount && denominator && std::abs(*denominator) > 1e-12
                    ? Json(*amount * 1e10 / *denominator) : Json(nullptr);
            };
            item["market_cap_ratio_pct"] = ratio(total);
            item["circulating_market_cap_ratio_pct"] = ratio(circulating);
            item["annual_profit_ratio_pct"] = ratio(profit);
        } else if (kind == "high-dividend") {
            date = text_value(row, "N001");
            item["fiscal_year_end"] = date;
            item["dividend_yuan"] = number(row, "N002");
            item["latest_annual_profit_yuan"] = number(row, "N003");
            item["payout_ratio_pct"] = number(row, "N004");
        } else if (kind == "hk-performance") {
            date = text_value(row, "rq");
            item["snapshot_date"] = date;
            item["close_price_hkd"] = number(row, "spj");
            item["turnover_yuan"] = number(row, "drcje");
            item["change_5d_pct"] = number(row, "wrzf");
            item["turnover_5d_yuan"] = number(row, "wrcje");
            item["change_20d_pct"] = number(row, "eszf");
            item["change_60d_pct"] = number(row, "lszf");
            item["change_month_pct"] = number(row, "byzf");
            item["change_ytd_pct"] = number(row, "nczf");
            item["pe"] = number(row, "syl");
        } else if (kind == "high-refinancing-lending") {
            date = text_value(row, "jyrq");
            item["trade_date"] = date;
            item["circulating_market_cap_yuan"] = number(row, "LTSZ");
            item["market_cap_yuan"] = number(row, "zsz");
            item["refinancing_lending_balance_10k_yuan"] = number(row, "zxye");
            item["refinancing_lending_balance_yuan"] = scaled(row, "zxye", 1e4);
            item["balance_ratio_pct"] = number(row, "yezb");
            item["refinancing_lending_10k_shares"] = number(row, "rqyl");
            item["refinancing_lending_shares"] = scaled(row, "rqyl", 1e4);
            item["has_lending_data"] = number_value(row, "zxye").has_value() ||
                number_value(row, "rqyl").has_value();
        } else {
            date = text_value(row, "N001");
            item["snapshot_date"] = date;
            item["market_cap_yuan"] = number(row, "N002");
            item["net_assets_yuan"] = number(row, "N003");
            item["price_to_book_ratio"] = number(row, "N004");
            item["controlling_shareholder"] = text_value(row, "N005");
            item["controlling_shareholder_nature"] = text_value(row, "N006");
            item["controlling_shareholder_pct"] = number(row, "N007");
            item["actual_controller"] = text_value(row, "N008");
            item["actual_controller_nature"] = text_value(row, "N009");
            item["actual_controller_pct"] = number(row, "N010");
        }

        item["date"] = date;
        item["security"] = security;
        const auto identity = security.is_null()
            ? std::string("market") : security.at("security_id").as_string();
        item["event_id"] = kind + ":" + identity + ":" + date + ":" +
            std::to_string(index++);
        result.push_back(std::move(item));
    }
    return result;
}

}  // namespace tdx
