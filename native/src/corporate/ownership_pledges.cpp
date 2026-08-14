#include "ownership_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>

namespace tdx {

using namespace ownership_detail;
Json normalize_pledge_latest_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("latest pledge rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 6)) continue;
        const auto id = row_market_id(row);
        if (!id) continue;
        Json item = Json::object();
        item["security"] = security_document(*id, code, securities);
        item["kind"] = "latest";
        item["kind_label"] = "最新质押";
        item["announcement_date"] = text_value(row, "date");
        item["relationship"] = text_value(row, "dgdzy1");
        item["single_capital_pct"] = number_or_null(number_value(row, "dgdzy2"));
        item["cumulative_shares"] = scaled_number(row, "dgdzy3", 10000.0);
        item["holder_shares"] = scaled_number(row, "dgdzy4", 10000.0);
        item["single_holder_pct"] = number_or_null(number_value(row, "dgdzy5"));
        item["restricted_shares"] = scaled_number(row, "yxsgf", 10000.0);
        item["unrestricted_shares"] = scaled_number(row, "wxsgf", 10000.0);
        item["pledge_count"] = number_or_null(number_value(row, "zybs"));
        item["cumulative_capital_pct"] = number_or_null(number_value(row, "zzgb"));
        item["risk_status"] = text_value(row, "ts");
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "announcement_date") >
                   text_value(right, "announcement_date");
        });
    return result;
}

Json normalize_pledge_risk_rows(
    const Json& rows, const std::string& kind,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("pledge risk rows must be an array");
    if (kind != "warning" && kind != "liquidation")
        throw Error("pledge risk kind must be warning or liquidation");
    const bool warning = kind == "warning";
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 6)) continue;
        const auto id = row_market_id(row);
        if (!id) continue;
        Json item = Json::object();
        item["security"] = security_document(*id, code, securities);
        item["kind"] = kind;
        item["kind_label"] = warning ? "预警提示" : "平仓提示";
        item["pledged_shares"] = scaled_number(row, "ljgs", 10000.0);
        item["cumulative_capital_pct"] = number_or_null(number_value(row, "zzgb"));
        item["price_range"] = text_value(row, warning ? "yjqj" : "pcqj");
        item["affected_shares"] = scaled_number(row, warning ? "yjgs" : "pcgs", 10000.0);
        item["affected_pct"] = number_or_null(number_value(row, warning ? "yjzb" : "pczb"));
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return number_value(left, "affected_shares").value_or(0.0) >
                   number_value(right, "affected_shares").value_or(0.0);
        });
    return result;
}

Json normalize_pledge_release_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("pledge release rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 6)) continue;
        const auto id = row_market_id(row);
        if (!id) continue;
        Json item = Json::object();
        item["security"] = security_document(*id, code, securities);
        item["kind"] = "release";
        item["kind_label"] = "最新解押";
        item["shareholder"] = text_value(row, "gdmc");
        item["relationship"] = text_value(row, "kggx");
        item["pledgee"] = text_value(row, "zyf");
        item["release_date"] = text_value(row, "jyrq");
        item["released_shares"] = scaled_number(row, "gs1", 10000.0);
        item["released_capital_pct"] = number_or_null(number_value(row, "zgb1"));
        item["released_holder_pct"] = number_or_null(number_value(row, "zsc1"));
        item["cumulative_shares"] = scaled_number(row, "gs", 10000.0);
        item["cumulative_capital_pct"] = number_or_null(number_value(row, "zgb"));
        item["cumulative_holder_pct"] = number_or_null(number_value(row, "zsc"));
        item["announcement_date"] = text_value(row, "ggrq");
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "release_date") > text_value(right, "release_date");
        });
    return result;
}

Json normalize_pledge_history_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("pledge history rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["pledge_date"] = text_value(row, "zyrq");
        item["shareholder"] = text_value(row, "gdmc");
        item["relationship"] = text_value(row, "kggx");
        item["pledgee"] = text_value(row, "zyf");
        item["shares"] = scaled_number(row, "zygs", 10000.0);
        item["single_holder_pct"] = number_or_null(number_value(row, "zcgb"));
        item["cumulative_holder_pct"] = number_or_null(number_value(row, "ljzy"));
        item["single_capital_pct"] = number_or_null(number_value(row, "zzgb"));
        item["cumulative_capital_pct"] = number_or_null(number_value(row, "ljzb"));
        item["description"] = text_value(row, "sy");
        item["maturity_date"] = text_value(row, "dqrq");
        item["previous_close"] = number_or_null(number_value(row, "maxp"));
        item["liquidation_line"] = number_or_null(number_value(row, "pcx"));
        item["warning_line"] = number_or_null(number_value(row, "yjx"));
        item["decline_to_warning_pct"] = number_or_null(number_value(row, "dyyj"));
        item["risk_status"] = text_value(row, "ts");
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "pledge_date") > text_value(right, "pledge_date");
        });
    return result;
}

Json normalize_ownership_statistics_rows(const Json& rows,
                                         const std::string& period) {
    if (!rows.is_array()) throw Error("ownership statistics rows must be an array");
    if (period != "month" && period != "year")
        throw Error("ownership statistics period must be month or year");
    const bool month = period == "month";
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["period"] = month ? text_value(row, "$ZQDM") : text_value(row, "NF");
        item["period_type"] = period;
        item["increase_amount_yuan"] = scaled_number(row, month ? "ZCE" : "NZCE", 100000000.0);
        item["decrease_amount_yuan"] = scaled_number(row, month ? "JCE" : "NJCE", 100000000.0);
        item["net_amount_yuan"] = scaled_number(row, month ? "JE" : "NJE", 100000000.0);
        item["increase_companies"] = number_or_null(number_value(row, month ? "ZCS" : "NZCS"));
        item["decrease_companies"] = number_or_null(number_value(row, month ? "JCS" : "NJCS"));
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "period") > text_value(right, "period");
        });
    return result;
}

Json normalize_ownership_change_count_trend_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("ownership change-count trend rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        auto period = text_value(row, "date");
        if (period.size() == 6 && digits(period, 6)) period.insert(4, "-");
        Json item = Json::object();
        item["period"] = period;
        item["increase_companies"] = number_or_null(number_value(row, "zcs"));
        item["decrease_companies"] = number_or_null(number_value(row, "jcs"));
        item["raw"] = row;
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "period") > text_value(right, "period");
        });
    return result;
}

Json normalize_pledge_month_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("pledge month rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["month"] = text_value(row, "date");
        item["company_count"] = number_or_null(number_value(row, "zygs"));
        item["total_capital_shares"] = scaled_number(row, "zgb", 10000.0);
        item["single_large_shares"] = scaled_number(row, "dgdzy", 10000.0);
        item["large_cumulative_shares"] = scaled_number(row, "dgdzy3", 10000.0);
        item["large_holder_shares"] = scaled_number(row, "cygf", 10000.0);
        item["restricted_shares"] = scaled_number(row, "yxsgf", 10000.0);
        item["unrestricted_shares"] = scaled_number(row, "wxsgf", 10000.0);
        item["pledge_count"] = number_or_null(number_value(row, "zybs"));
        const auto total = number_value(row, "zgb");
        const auto pledged_restricted = number_value(row, "yxsgf").value_or(0.0);
        const auto pledged_unrestricted = number_value(row, "wxsgf").value_or(0.0);
        item["cumulative_capital_pct"] = total && *total > 0.0
            ? Json((pledged_restricted + pledged_unrestricted) / *total * 100.0)
            : Json(nullptr);
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "month") > text_value(right, "month");
        });
    return result;
}

Json normalize_pledge_institution_rows(const Json& rows,
                                       const std::string& category) {
    if (!rows.is_array()) throw Error("pledge institution rows must be an array");
    if (category != "trust" && category != "broker")
        throw Error("pledge institution category must be trust or broker");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto id = text_value(row, "$ZQDM");
        if (!safe_identifier(id)) continue;
        Json item = Json::object();
        item["institution_id"] = id;
        item["category"] = category;
        item["category_label"] = category == "trust" ? "信托" : "券商";
        item["name"] = text_value(row, "zyf");
        item["pledge_count"] = number_or_null(number_value(row, "zybs"));
        item["company_count"] = number_or_null(number_value(row, "zygss"));
        item["market_value_yuan"] = number_or_null(number_value(row, "zysz"));
        item["warning_value_yuan"] = number_or_null(number_value(row, "tsyj1"));
        item["warning_pct"] = number_or_null(number_value(row, "tsyj2"));
        item["liquidation_value_yuan"] = number_or_null(number_value(row, "tspc1"));
        item["liquidation_pct"] = number_or_null(number_value(row, "tspc2"));
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return number_value(left, "market_value_yuan").value_or(0.0) >
                   number_value(right, "market_value_yuan").value_or(0.0);
        });
    return result;
}

Json normalize_pledge_institution_detail_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("pledge institution detail rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 6)) continue;
        const auto id = row_market_id(row);
        if (!id) continue;
        Json item = Json::object();
        item["security"] = security_document(*id, code, securities);
        item["announcement_date"] = text_value(row, "ggrq");
        item["pledge_date"] = text_value(row, "zyrq");
        item["maturity_date"] = text_value(row, "dqrq");
        item["shareholder"] = text_value(row, "gdmc");
        item["relationship"] = text_value(row, "kggx");
        item["shares"] = scaled_number(row, "zygs", 10000.0);
        item["holder_pct"] = number_or_null(number_value(row, "zcgb"));
        item["capital_pct"] = number_or_null(number_value(row, "zzgb"));
        item["liquidation_line"] = number_or_null(number_value(row, "pcx"));
        item["warning_line"] = number_or_null(number_value(row, "yjx"));
        item["risk_status"] = text_value(row, "ts");
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "announcement_date") >
                   text_value(right, "announcement_date");
        });
    return result;
}

}  // namespace tdx