#include "financial_insights_internal.hpp"

#include <cmath>

namespace tdx {

Json normalize_financial_insight_rows(
    const std::string& resource, const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("financial-insights rows must be an array");
    const auto& definition =
        detail::financial_insights::find_resource(resource);
    const std::string kind(definition.kind);
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        auto security = detail::financial_insights::security_document(detail::financial_insights::integer_value(row, "$SC"),
                                          detail::financial_insights::text_value(row, "$ZQDM"), securities);
        if (security.is_null()) continue;
        Json item = Json::object();
        item["kind"] = kind;
        item["kind_label"] = std::string(definition.label);
        item["security"] = std::move(security);
        item["source_resource"] = resource;
        item["raw"] = row;
        if (kind == "buffett-quality") {
            item["report_period"] = detail::financial_insights::text_value(row, "bgq");
            detail::financial_insights::add_number(item, row, "net_profit_yuan", "jlr");
            detail::financial_insights::add_number(item, row, "pe", "syl");
            detail::financial_insights::add_number(item, row, "roe_pct", "roe");
            detail::financial_insights::add_number(item, row, "roe_qualified_years", "cs");
            detail::financial_insights::set_signal(item, item.at("roe_qualified_years"), item.at("net_profit_yuan"), item.at("roe_pct"));
        } else if (kind == "high-bonus-potential") {
            item["report_period"] = detail::financial_insights::text_value(row, "date");
            item["listing_date"] = detail::financial_insights::text_value(row, "date1");
            detail::financial_insights::add_number(item, row, "capital_reserve_per_share_yuan", "zbgj");
            detail::financial_insights::add_number(item, row, "undistributed_profit_per_share_yuan", "wfplr");
            detail::financial_insights::add_number(item, row, "parent_net_profit_yuan", "jlr1");
            detail::financial_insights::add_number(item, row, "parent_net_profit_yoy_pct", "jlr3");
            detail::financial_insights::add_number(item, row, "latest_share_capital_10k", "ZX1");
            item["latest_share_capital_shares"] = detail::financial_insights::scaled(row, "ZX1", 10000.0);
            item["forecast_type"] = detail::financial_insights::text_value(row, "ZX3");
            item["forecast_detail"] = detail::financial_insights::text_value(row, "ZX2");
            detail::financial_insights::set_signal(item, item.at("capital_reserve_per_share_yuan"),
                       item.at("parent_net_profit_yuan"), item.at("parent_net_profit_yoy_pct"));
        } else if (kind == "investment-property") {
            item["report_period"] = detail::financial_insights::text_value(row, "bgq");
            detail::financial_insights::add_number(item, row, "investment_property_yuan", "cyje1");
            detail::financial_insights::add_number(item, row, "prior_investment_property_yuan", "cyje2");
            detail::financial_insights::add_number(item, row, "year_ago_investment_property_yuan", "cyje3");
            item["investment_property_change_yuan"] = detail::financial_insights::difference(row, "cyje1", "cyje2");
            item["investment_property_qoq_pct"] = detail::financial_insights::change_pct(row, "cyje1", "cyje2");
            item["investment_property_yoy_change_yuan"] = detail::financial_insights::difference(row, "cyje1", "cyje3");
            item["investment_property_yoy_pct"] = detail::financial_insights::change_pct(row, "cyje1", "cyje3");
            detail::financial_insights::set_signal(item, item.at("investment_property_yuan"),
                       item.at("investment_property_yuan"), item.at("investment_property_yoy_pct"));
        } else if (kind == "low-price-sales") {
            detail::financial_insights::add_number(item, row, "expected_revenue_cagr_3y_pct", "ETSR");
            detail::financial_insights::add_number(item, row, "expected_net_margin_pct", "ENPR");
            detail::financial_insights::add_number(item, row, "price_to_sales", "PS");
            detail::financial_insights::add_number(item, row, "price_to_rd", "PR");
            detail::financial_insights::add_number(item, row, "market_cap_yuan", "GPSZ");
            detail::financial_insights::add_number(item, row, "pe", "PE");
            detail::financial_insights::add_number(item, row, "pb", "PB");
            detail::financial_insights::add_number(item, row, "price_to_cash_flow", "PCF");
            detail::financial_insights::add_number(item, row, "net_margin_pct", "NPR");
            const auto ps = detail::financial_insights::number_value(row, "PS");
            const Json signal = ps ? Json(-*ps) : Json(nullptr);
            detail::financial_insights::set_signal(item, signal, item.at("market_cap_yuan"), item.at("expected_revenue_cagr_3y_pct"));
        } else if (kind == "dividend-shortfall") {
            detail::financial_insights::add_number(item, row, "latest_dividend_10k_yuan", "N001");
            item["latest_dividend_yuan"] = detail::financial_insights::scaled(row, "N001", 10000.0);
            detail::financial_insights::add_number(item, row, "latest_net_profit_10k_yuan", "N002");
            item["latest_net_profit_yuan"] = detail::financial_insights::scaled(row, "N002", 10000.0);
            detail::financial_insights::add_number(item, row, "undistributed_profit_yuan", "N003");
            detail::financial_insights::add_number(item, row, "dividend_3y_10k_yuan", "N004");
            item["dividend_3y_yuan"] = detail::financial_insights::scaled(row, "N004", 10000.0);
            detail::financial_insights::add_number(item, row, "average_profit_3y_10k_yuan", "N005");
            item["average_profit_3y_yuan"] = detail::financial_insights::scaled(row, "N005", 10000.0);
            item["dividend_to_average_profit_3y_pct"] = detail::financial_insights::ratio_pct(row, "N004", "N005");
            detail::financial_insights::add_number(item, row, "rd_investment_3y_yuan", "N007");
            detail::financial_insights::add_number(item, row, "revenue_3y_yuan", "N008");
            item["rd_to_revenue_3y_pct"] = detail::financial_insights::ratio_pct(row, "N007", "N008");
            detail::financial_insights::set_signal(item, item.at("dividend_to_average_profit_3y_pct"),
                       item.at("latest_net_profit_yuan"), item.at("rd_to_revenue_3y_pct"));
        } else if (kind == "equity-investment") {
            item["report_period"] = detail::financial_insights::text_value(row, "DATE");
            detail::financial_insights::add_number(item, row, "a_share_investment_count", "ZQS1");
            detail::financial_insights::add_number(item, row, "other_investment_count", "ZQS2");
            detail::financial_insights::add_number(item, row, "investment_total_10k_yuan", "ZJE");
            item["investment_total_yuan"] = detail::financial_insights::scaled(row, "ZJE", 10000.0);
            detail::financial_insights::add_number(item, row, "investment_return_10k_yuan", "ZSY");
            item["investment_return_yuan"] = detail::financial_insights::scaled(row, "ZSY", 10000.0);
            item["investment_return_pct"] = detail::financial_insights::ratio_pct(row, "ZSY", "ZJE");
            detail::financial_insights::set_signal(item, item.at("investment_total_yuan"),
                       item.at("investment_total_yuan"), item.at("investment_return_pct"));
        } else if (kind == "cash-above-market-cap") {
            item["report_period"] = detail::financial_insights::text_value(row, "date");
            detail::financial_insights::add_number(item, row, "market_cap_yuan", "zsz");
            detail::financial_insights::add_number(item, row, "cash_yuan", "hbzj");
            item["cash_excess_yuan"] = detail::financial_insights::difference(row, "hbzj", "zsz");
            item["cash_to_market_cap_pct"] = detail::financial_insights::ratio_pct(row, "hbzj", "zsz");
            detail::financial_insights::add_number(item, row, "debt_ratio_source", "zcfzl");
            item["debt_ratio_pct"] = detail::financial_insights::scaled(row, "zcfzl", 100.0);
            detail::financial_insights::add_number(item, row, "dividend_yield_pct", "gxl");
            detail::financial_insights::add_number(item, row, "cumulative_dividend_100m_yuan", "ljfh");
            item["cumulative_dividend_yuan"] = detail::financial_insights::scaled(row, "ljfh", 100000000.0);
            detail::financial_insights::add_number(item, row, "dividend_count", "ljfhcs");
            detail::financial_insights::add_number(item, row, "cumulative_raised_100m_yuan", "ljmj");
            item["cumulative_raised_yuan"] = detail::financial_insights::scaled(row, "ljmj", 100000000.0);
            detail::financial_insights::add_number(item, row, "fundraising_count", "ljmjcs");
            item["dividend_to_fundraising_ratio"] = [&]() -> Json {
                const auto d = detail::financial_insights::number_value(row, "ljfh"), f = detail::financial_insights::number_value(row, "ljmj");
                return d && f && *f != 0.0 ? Json(*d / *f) : Json(nullptr);
            }();
            detail::financial_insights::add_number(item, row, "institutions_6m", "jgsl");
            detail::financial_insights::add_number(item, row, "consensus_rating", "zhpj");
            detail::financial_insights::set_signal(item, item.at("cash_excess_yuan"), item.at("cash_yuan"),
                       item.at("cash_to_market_cap_pct"));
        } else if (kind == "high-receivables") {
            item["report_period"] = detail::financial_insights::text_value(row, "bgq");
            detail::financial_insights::add_number(item, row, "receivables_yuan", "yszk");
            detail::financial_insights::add_number(item, row, "market_cap_yuan", "zsz");
            detail::financial_insights::add_number(item, row, "receivables_to_market_cap_pct", "zb");
            detail::financial_insights::set_signal(item, item.at("receivables_to_market_cap_pct"),
                       item.at("receivables_yuan"), item.at("receivables_to_market_cap_pct"));
        } else if (kind == "profit-warning") {
            item["report_period"] = detail::financial_insights::text_value(row, "N001");
            item["report_type"] = detail::financial_insights::text_value(row, "N002");
            detail::financial_insights::add_number(item, row, "net_assets_yuan", "N003");
            detail::financial_insights::add_number(item, row, "operating_revenue_yuan", "N004");
            detail::financial_insights::add_number(item, row, "actual_profit_yuan", "N005");
            item["forecast_period"] = detail::financial_insights::text_value(row, "N006");
            detail::financial_insights::add_number(item, row, "forecast_profit_lower_yuan", "N007");
            detail::financial_insights::add_number(item, row, "forecast_profit_upper_yuan", "N008");
            item["forecast_profit_midpoint_yuan"] = detail::financial_insights::midpoint(row, "N007", "N008");
            item["forecast_disclosure_date"] = detail::financial_insights::text_value(row, "N009");
            item["has_forecast"] = !detail::financial_insights::text_value(row, "N006").empty() &&
                detail::financial_insights::number_value(row, "N007").has_value() &&
                detail::financial_insights::number_value(row, "N008").has_value();
            const auto actual = detail::financial_insights::number_value(row, "N005");
            item["actual_profit_positive"] = actual && *actual > 0.0;
            detail::financial_insights::set_signal(item, item.at("actual_profit_yuan"),
                       item.at("actual_profit_yuan"), Json(nullptr));
        } else if (kind == "cash-flow-quality") {
            item["report_period"] = detail::financial_insights::text_value(row, "bgq");
            detail::financial_insights::add_number(item, row, "free_cash_flow_yuan", "zyxjl");
            detail::financial_insights::add_number(item, row, "operating_cash_flow_per_share_yuan", "xjl");
            detail::financial_insights::add_number(item, row, "operating_cash_flow_yuan", "xjje");
            detail::financial_insights::add_number(item, row, "operating_cash_to_short_debt_pct", "jxjbl");
            detail::financial_insights::add_number(item, row, "operating_cash_to_net_profit_pct", "jlrbl");
            detail::financial_insights::add_number(item, row, "debt_ratio_pct", "zcfzl");
            detail::financial_insights::add_number(item, row, "current_ratio_source", "ldbl");
            detail::financial_insights::add_number(item, row, "quick_ratio_source", "sdbl");
            detail::financial_insights::add_number(item, row, "interest_coverage_ratio", "lxbzbs");
            detail::financial_insights::add_number(item, row, "inventory_to_current_assets_pct", "chzcbl");
            detail::financial_insights::add_number(item, row, "pe", "syl");
            const auto per_share = detail::financial_insights::number_value(row, "xjl");
            const auto short_debt = detail::financial_insights::number_value(row, "jxjbl");
            const auto net_profit = detail::financial_insights::number_value(row, "jlrbl");
            const auto inventory = detail::financial_insights::number_value(row, "chzcbl");
            Json criteria = Json::object();
            criteria["positive_operating_cash_per_share"] =
                per_share && *per_share > 0.0;
            criteria["cash_to_short_debt_above_50_pct"] =
                short_debt && *short_debt > 50.0;
            criteria["cash_to_net_profit_above_100_pct"] =
                net_profit && *net_profit > 100.0;
            criteria["inventory_to_current_assets_below_50_pct"] =
                inventory && *inventory < 50.0;
            item["screen_criteria"] = std::move(criteria);
            item["screen_criteria_complete"] =
                per_share && *per_share > 0.0 && short_debt && *short_debt > 50.0 &&
                net_profit && *net_profit > 100.0 && inventory && *inventory < 50.0;
            detail::financial_insights::set_signal(item, item.at("operating_cash_flow_per_share_yuan"),
                       item.at("free_cash_flow_yuan"),
                       item.at("operating_cash_to_net_profit_pct"));
        } else if (kind == "earnings-reversal") {
            item["report_period"] = detail::financial_insights::text_value(row, "BGQ");
            item["report_type"] = detail::financial_insights::text_value(row, "bglx");
            item["consensus_date"] = detail::financial_insights::text_value(row, "ZXRQ");
            detail::financial_insights::add_number(item, row, "consensus_institutions", "JGSL");
            detail::financial_insights::add_number(item, row, "consensus_rating", "ZHPJ");
            detail::financial_insights::add_number(item, row, "target_price_yuan", "MBJ");
            detail::financial_insights::add_number(item, row, "disclosed_net_profit_yuan", "JLR1");
            detail::financial_insights::add_number(item, row, "forecast_net_profit_yuan", "JLR2");
            detail::financial_insights::add_number(item, row, "profit_growth_pct", "JLRZZL");
            item["profit_growth_recalculated_pct"] =
                detail::financial_insights::change_pct_absolute_base(row, "JLR2", "JLR1");
            detail::financial_insights::add_number(item, row, "disclosed_revenue_yuan", "YYSR1");
            detail::financial_insights::add_number(item, row, "forecast_revenue_yuan", "YYSR2");
            detail::financial_insights::add_number(item, row, "revenue_growth_pct", "YSZZL");
            const auto disclosed = detail::financial_insights::number_value(row, "JLR1");
            const auto forecast = detail::financial_insights::number_value(row, "JLR2");
            item["profit_reversal"] = disclosed && forecast &&
                *disclosed <= 0.0 && *forecast > 0.0;
            detail::financial_insights::set_signal(item, item.at("profit_growth_pct"),
                       item.at("forecast_net_profit_yuan"),
                       item.at("profit_growth_pct"));
        } else if (kind == "steady-growth") {
            detail::financial_insights::add_number(item, row, "three_year_reference_close_yuan", "price");
            detail::financial_insights::add_number(item, row, "listed_since_return_pct", "sszf");
            detail::financial_insights::add_number(item, row, "cumulative_dividend_100m_yuan", "ljfh");
            item["cumulative_dividend_yuan"] = detail::financial_insights::scaled(row, "ljfh", 100000000.0);
            detail::financial_insights::add_number(item, row, "dividend_count", "ljfhcs");
            detail::financial_insights::add_number(item, row, "cumulative_net_profit_yuan", "zjlr");
            const auto dividend = detail::financial_insights::number_value(row, "ljfh");
            const auto profit = detail::financial_insights::number_value(row, "zjlr");
            item["dividend_payout_pct"] = dividend && profit && *profit != 0.0
                ? Json(*dividend * 100000000.0 * 100.0 / *profit)
                : Json(nullptr);
            detail::financial_insights::add_number(item, row, "beta", "beta");
            detail::financial_insights::add_number(item, row, "pe", "pe_ttm");
            detail::financial_insights::set_signal(item, item.at("listed_since_return_pct"),
                       item.at("cumulative_dividend_yuan"),
                       item.at("dividend_payout_pct"));
        } else if (kind == "quality-growth") {
            item["report_period"] = detail::financial_insights::text_value(row, "zxbgq");
            detail::financial_insights::add_number(item, row, "revenue_t_minus_2_yuan", "dsnyysr");
            detail::financial_insights::add_number(item, row, "revenue_t_minus_1_yuan", "denyysr");
            detail::financial_insights::add_number(item, row, "revenue_t_yuan", "dynyysr");
            item["revenue_growth_t_minus_1_pct"] =
                detail::financial_insights::change_pct(row, "denyysr", "dsnyysr");
            item["revenue_growth_t_pct"] =
                detail::financial_insights::change_pct(row, "dynyysr", "denyysr");
            detail::financial_insights::add_number(item, row, "selling_expense_rate_t_minus_2_pct", "dsnssfyl");
            detail::financial_insights::add_number(item, row, "selling_expense_rate_t_minus_1_pct", "denssfyl");
            detail::financial_insights::add_number(item, row, "selling_expense_rate_t_pct", "dynssfyl");
            detail::financial_insights::add_number(item, row, "gross_margin_t_minus_2_pct", "dsnmll");
            detail::financial_insights::add_number(item, row, "gross_margin_t_minus_1_pct", "denmll");
            detail::financial_insights::add_number(item, row, "gross_margin_t_pct", "dynmll");
            detail::financial_insights::add_number(item, row, "rd_expense_t_minus_2_yuan", "dsnyffy");
            detail::financial_insights::add_number(item, row, "rd_expense_t_minus_1_yuan", "denyffy");
            detail::financial_insights::add_number(item, row, "rd_expense_t_yuan", "dynyffy");
            item["rd_growth_t_minus_1_pct"] =
                detail::financial_insights::change_pct(row, "denyffy", "dsnyffy");
            item["rd_growth_t_pct"] = detail::financial_insights::change_pct(row, "dynyffy", "denyffy");
            const auto revenue0 = detail::financial_insights::number_value(row, "dsnyysr");
            const auto revenue1 = detail::financial_insights::number_value(row, "denyysr");
            const auto revenue2 = detail::financial_insights::number_value(row, "dynyysr");
            const auto sales0 = detail::financial_insights::number_value(row, "dsnssfyl");
            const auto sales1 = detail::financial_insights::number_value(row, "denssfyl");
            const auto sales2 = detail::financial_insights::number_value(row, "dynssfyl");
            const auto margin0 = detail::financial_insights::number_value(row, "dsnmll");
            const auto margin1 = detail::financial_insights::number_value(row, "denmll");
            const auto margin2 = detail::financial_insights::number_value(row, "dynmll");
            const auto rd0 = detail::financial_insights::number_value(row, "dsnyffy");
            const auto rd1 = detail::financial_insights::number_value(row, "denyffy");
            const auto rd2 = detail::financial_insights::number_value(row, "dynyffy");
            Json criteria = Json::object();
            criteria["revenue_increases"] = revenue0 && revenue1 && revenue2 &&
                *revenue0 < *revenue1 && *revenue1 < *revenue2;
            criteria["rd_expense_increases"] = rd0 && rd1 && rd2 &&
                *rd0 < *rd1 && *rd1 < *rd2;
            criteria["gross_margin_increases"] = margin0 && margin1 && margin2 &&
                *margin0 < *margin1 && *margin1 < *margin2;
            criteria["selling_expense_rate_decreases"] = sales0 && sales1 && sales2 &&
                *sales0 > *sales1 && *sales1 > *sales2;
            item["screen_criteria"] = criteria;
            item["screen_criteria_complete"] =
                criteria.at("revenue_increases").as_bool() &&
                criteria.at("rd_expense_increases").as_bool() &&
                criteria.at("gross_margin_increases").as_bool() &&
                criteria.at("selling_expense_rate_decreases").as_bool();
            detail::financial_insights::set_signal(item, item.at("revenue_growth_t_pct"),
                       item.at("revenue_t_yuan"), item.at("gross_margin_t_pct"));
        } else if (kind == "profit-breakout") {
            item["report_period"] = detail::financial_insights::text_value(row, "zxrq");
            item["prior_period"] = detail::financial_insights::text_value(row, "qntq");
            item["comparison_period"] = detail::financial_insights::text_value(row, "bjqrq");
            detail::financial_insights::add_number(item, row, "latest_or_forecast_profit_yuan", "bqlr");
            detail::financial_insights::add_number(item, row, "prior_period_adjusted_profit_yuan", "qntqlr");
            detail::financial_insights::add_number(item, row, "comparison_adjusted_profit_yuan", "sqlr");
            item["profit_yoy_change_yuan"] = detail::financial_insights::difference(row, "bqlr", "qntqlr");
            item["profit_yoy_growth_pct"] = detail::financial_insights::change_pct(row, "bqlr", "qntqlr");
            item["profit_breakout_change_yuan"] = detail::financial_insights::difference(row, "bqlr", "sqlr");
            item["profit_breakout_growth_pct"] = detail::financial_insights::change_pct(row, "bqlr", "sqlr");
            detail::financial_insights::set_signal(item, item.at("profit_breakout_growth_pct"),
                       item.at("latest_or_forecast_profit_yuan"),
                       item.at("profit_yoy_growth_pct"));
        } else {
            item["report_period"] = detail::financial_insights::text_value(row, "fhdate");
            item["announcement_date"] = detail::financial_insights::text_value(row, "ggdate");
            item["record_date"] = detail::financial_insights::text_value(row, "djdate");
            item["ex_dividend_date"] = detail::financial_insights::text_value(row, "qxdate");
            item["plan_stage"] = detail::financial_insights::text_value(row, "fajd");
            item["industry"] = detail::financial_insights::text_value(row, "hy");
            detail::financial_insights::add_number(item, row, "stock_transfer_per_10_shares", "szbl");
            item["stock_transfer_per_share"] = [&]() -> Json {
                const auto value = detail::financial_insights::number_value(row, "szbl");
                return value ? Json(*value / 10.0) : Json(nullptr);
            }();
            detail::financial_insights::add_number(item, row, "cash_dividend_per_10_shares_yuan", "xjfh");
            item["cash_dividend_per_share_yuan"] = [&]() -> Json {
                const auto value = detail::financial_insights::number_value(row, "xjfh");
                return value ? Json(*value / 10.0) : Json(nullptr);
            }();
            detail::financial_insights::add_number(item, row, "return_1w_pct", "WeekPrice");
            detail::financial_insights::add_number(item, row, "return_1m_pct", "MonthPrice");
            detail::financial_insights::add_number(item, row, "return_3m_pct", "QuarterPrice");
            detail::financial_insights::set_signal(item, item.at("cash_dividend_per_10_shares_yuan"),
                       Json(nullptr), item.at("return_3m_pct"));
        }
        item["record_id"] = kind + ":" +
            item.at("security").at("security_id").as_string() + ":" +
            detail::financial_insights::text_value(item, "report_period");
        result.push_back(std::move(item));
    }
    return result;
}

}  // namespace tdx
