#include "institution_analysis_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace tdx::institution_analysis_detail {

Json holding_data(const ViewSpec& view, const Json& raw) {
    double shares = 0, change = 0;
    const bool has_shares = parse_number(raw, "gs", shares);
    const bool has_change = parse_number(raw, "gs_bd", change);
    const double total_scale = view.total_ratio_divide_100 ? 0.01 : 1.0;
    Json data = Json::object();
    data["report_date"] = copy_value(raw, "bgq");
    data["holding_market_value"] = number_or_null(raw, "sz");
    data["institution_count"] = number_or_null(raw, "jgsl");
    data["institution_count_change"] = number_or_null(raw, "jgsl_bd");
    data["holding_shares"] = number_or_null(raw, "gs");
    data["holding_shares_change"] = number_or_null(raw, "gs_bd");
    data["holding_shares_change_ratio"] = has_shares && has_change
        ? quotient_or_null(change, shares - change) : Json(nullptr);
    data["float_share_ratio"] = number_or_null(raw, "zltgb");
    data["float_share_ratio_change"] = number_or_null(raw, "zltgb_bd");
    data["total_share_ratio"] = number_or_null(raw, "zzgbb", total_scale);
    data["total_share_ratio_change"] = number_or_null(raw, "zzgbb_bd", total_scale);
    return data;
}

Json normalize_data(const ViewSpec& view, const Json& raw) {
    if (view.layout == Layout::holdings) return holding_data(view, raw);
    if (view.layout == Layout::social_security_summary) {
        Json data = Json::object();
        data["report_date"] = copy_value(raw, "bgq");
        data["institution_count"] = number_or_null(raw, "jgsl");
        data["holding_shares"] = number_or_null(raw, "ccgs");
        data["total_share_pct"] = number_or_null(raw, "zzgb");
        return data;
    }
    if (view.layout == Layout::development_bank) {
        Json data = Json::object();
        data["report_date"] = copy_value(raw, "date");
        data["shareholder_position"] = copy_value(raw, "cgxx");
        data["holding_detail"] = copy_value(raw, "xxsm");
        return data;
    }
    if (view.layout == Layout::named_holding) {
        Json data = Json::object();
        data["report_date"] = copy_value(raw, "date");
        data["shareholder_position"] = copy_value(raw, "cgxx");
        return data;
    }
    if (view.layout == Layout::named_holding_summary) {
        Json data = Json::object();
        data["report_date"] = copy_value(raw, "DATE");
        data["holding_shares_10k"] = number_or_null(raw, "SL");
        data["holding_shares"] = number_or_null(raw, "SL", 10000.0);
        data["holding_type"] = copy_value(raw, "LX");
        data["float_share_pct"] = number_or_null(raw, "ZB");
        data["holding_market_value_10k"] = number_or_null(raw, "CGSZ");
        data["holding_market_value"] = number_or_null(raw, "CGSZ", 10000.0);
        data["net_profit"] = number_or_null(raw, "bqlr");
        data["net_profit_growth_pct"] = number_or_null(raw, "JLR");
        data["industry"] = copy_value(raw, "HY");
        return data;
    }
    if (view.layout == Layout::stake_building) {
        double start = 0.0, end = 0.0;
        const bool comparable = parse_number(raw, "price1", start) &&
            parse_number(raw, "price2", end) && start != 0.0;
        Json data = Json::object();
        data["report_date"] = copy_value(raw, "ggrq");
        data["announcement_date"] = copy_value(raw, "ggrq");
        data["start_date"] = copy_value(raw, "qsr");
        data["end_date"] = copy_value(raw, "jzr");
        data["shareholder"] = copy_value(raw, "gd");
        data["start_adjusted_close"] = number_or_null(raw, "price1");
        data["end_adjusted_close"] = number_or_null(raw, "price2");
        data["period_return_pct"] = comparable
            ? Json((end / start - 1.0) * 100.0) : Json(nullptr);
        data["average_price"] = number_or_null(raw, "cjjj");
        data["increase_shares"] = number_or_null(raw, "zcgs");
        data["increase_total_pct"] = number_or_null(raw, "zczb");
        data["post_holding_shares"] = number_or_null(raw, "zchgs");
        data["post_holding_total_pct"] = number_or_null(raw, "zchzb");
        data["further_increase"] = copy_value(raw, "sfjxzc");
        data["insurance_capital"] = copy_value(raw, "sfwxz");
        return data;
    }
    if (view.layout == Layout::crowded_oversold) {
        auto data = holding_data(view, raw);
        data["one_year_high_adjusted"] = number_or_null(raw, "price");
        data["latest_close_adjusted"] = number_or_null(raw, "price1");
        data["drawdown_from_high_pct"] = number_or_null(raw, "ngdzj");
        return data;
    }
    if (view.layout == Layout::pensions) {
        Json data = Json::object();
        data["report_date"] = copy_value(raw, "bgq");
        data["holding_shares_10k"] = number_or_null(raw, "gs");
        data["float_share_pct"] = number_or_null(raw, "zltgb");
        data["holding_market_value_10k"] = number_or_null(raw, "sz");
        data["net_profit"] = number_or_null(raw, "bqlr");
        data["net_profit_growth_pct"] = number_or_null(raw, "JLR");
        return data;
    }
    if (view.layout == Layout::exclusive_funds) {
        Json data = Json::object();
        data["fund_management_company"] = copy_value(raw, "glgs");
        data["report_date"] = copy_value(raw, "bgq");
        data["holding_shares_10k"] = number_or_null(raw, "cgsl");
        data["holding_shares"] = number_or_null(raw, "cgsl", 10000.0);
        data["float_share_pct"] = number_or_null(raw, "zb");
        return data;
    }
    if (view.layout == Layout::notable_private_funds) {
        Json data = Json::object();
        data["private_fund_manager"] = copy_value(raw, "mc");
        data["report_date"] = copy_value(raw, "bgq");
        data["holding_market_value_10k"] = number_or_null(raw, "sz");
        data["holding_market_value"] = number_or_null(raw, "sz", 10000.0);
        data["float_share_pct"] = number_or_null(raw, "zb");
        data["industry"] = copy_value(raw, "hy");
        return data;
    }
    if (view.layout == Layout::national_team) {
        double csf = 0, huijin = 0, combined = 0;
        const bool has_csf = parse_number(raw, "zjcg", csf);
        const bool has_huijin = parse_number(raw, "hjcg", huijin);
        const bool has_combined = parse_number(raw, "hj", combined);
        const double calculated = (has_csf ? csf : 0.0) +
                                  (has_huijin ? huijin : 0.0);
        Json data = Json::object();
        data["report_date"] = copy_value(raw, "date");
        data["china_securities_finance_ratio_pct"] = number_or_null(raw, "zjcg");
        data["central_huijin_ratio_pct"] = number_or_null(raw, "hjcg");
        data["combined_ratio_pct"] = number_or_null(raw, "hj");
        data["calculated_combined_ratio_pct"] =
            has_csf || has_huijin ? Json(calculated) : Json(nullptr);
        data["combined_ratio_formula_matches"] = has_combined && (has_csf || has_huijin)
            ? Json(std::abs(combined - calculated) <= 0.011) : Json(nullptr);
        data["holder_rank"] = copy_value(raw, "gdmc");
        data["industry"] = copy_value(raw, "hy");
        return data;
    }
    double total = 0, floating = 0, institutions = 0, major = 0;
    const bool complete = parse_number(raw, "bgqzgb", total) &&
        parse_number(raw, "bgqltgb", floating) && parse_number(raw, "gsll", institutions) &&
        parse_number(raw, "tdgs", major);
    const double free_float = complete ? floating - institutions - major : 0;
    Json data = Json::object();
    data["report_date"] = copy_value(raw, "bgq");
    data["report_total_shares"] = number_or_null(raw, "bgqzgb");
    data["report_float_shares"] = number_or_null(raw, "bgqltgb");
    data["free_float_shares"] = complete ? Json(free_float) : Json(nullptr);
    data["free_float_to_float_ratio"] = complete
        ? quotient_or_null(free_float, floating) : Json(nullptr);
    data["free_float_to_total_ratio"] = complete
        ? quotient_or_null(free_float, total) : Json(nullptr);
    data["institution_shares"] = number_or_null(raw, "gsll");
    data["institution_to_float_ratio"] = complete
        ? quotient_or_null(institutions, floating) : Json(nullptr);
    data["institution_to_total_ratio"] = complete
        ? quotient_or_null(institutions, total) : Json(nullptr);
    data["major_holder_shares"] = number_or_null(raw, "tdgs");
    data["major_holder_to_float_ratio"] = complete
        ? quotient_or_null(major, floating) : Json(nullptr);
    data["major_holder_to_total_ratio"] = complete
        ? quotient_or_null(major, total) : Json(nullptr);
    return data;
}

Json normalize_row(const ViewSpec& view, const Json& raw,
                   const std::map<std::pair<int, std::string>, Security>& securities) {
    const int id = market_id(text_value(raw, "$SC"));
    const auto code = text_value(raw, "$ZQDM");
    if (!six_digits(code)) throw Error("invalid CGFX security code");
    const auto found = securities.find({id, code});
    Json result = Json::object();
    result["market"] = market_name(id);
    result["market_id"] = id;
    result["code"] = code;
    result["security_id"] = market_prefix(id) + code;
    result["name"] = found == securities.end() ? "" : found->second.name;
    result["name_resolved"] = found != securities.end();
    result["data"] = normalize_data(view, raw);
    result["raw"] = raw;
    return result;
}

}  // namespace tdx::institution_analysis_detail
