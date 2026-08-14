#include "ownership_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>

namespace tdx {

using namespace ownership_detail;
Json normalize_ownership_change_rows(
    const Json& rows, const std::string& direction,
    const std::map<std::pair<int, std::string>, Security>& securities,
    bool with_security) {
    if (!rows.is_array()) throw Error("ownership change rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        if (with_security) {
            const auto code = text_value(row, "$ZQDM");
            if (!digits(code, 6)) continue;
            const auto id = row_market_id(row);
            if (!id) continue;
            item["security"] = security_document(*id, code, securities);
        }
        auto normalized_direction = direction;
        const auto shares = number_value(row, "bdgs");
        if (normalized_direction == "auto")
            normalized_direction = shares && *shares < 0.0 ? "decrease" : "increase";
        item["direction"] = normalized_direction;
        item["direction_label"] = normalized_direction == "decrease" ? "减持" : "增持";
        item["start_date"] = text_value(row, "qsrq");
        item["end_date"] = text_value(row, "jzrq");
        item["announcement_date"] = text_value(row, "ggrq");
        item["change_shares"] = scaled_number(row, "bdgs", 10000.0);
        item["average_price"] = number_or_null(number_value(row, "cjjj"));
        item["holding_after_shares"] = scaled_number(row, "bdhcg", 10000.0);
        item["holding_after_capital_pct"] = number_or_null(number_value(row, "bdhzb"));
        item["actor"] = text_value(row, "bdr");
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            if (text_value(left, "announcement_date") !=
                text_value(right, "announcement_date"))
                return text_value(left, "announcement_date") >
                       text_value(right, "announcement_date");
            return text_value(left, "end_date") > text_value(right, "end_date");
        });
    return result;
}

Json normalize_ownership_plan_rows(
    const Json& rows, const std::string& direction,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("ownership plan rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 6)) continue;
        const auto id = row_market_id(row);
        if (!id) continue;
        Json item = Json::object();
        item["security"] = security_document(*id, code, securities);
        item["direction"] = direction;
        item["direction_label"] = direction == "decrease" ? "拟减持" : "拟增持";
        item["announcement_date"] = text_value(row, "ggrq");
        item["announcement_close"] = number_or_null(number_value(row, "price"));
        item["start_date"] = text_value(row, "qsrq");
        item["end_date"] = text_value(row, "jzrq");
        item["range"] = text_value(row, "fw");
        item["scale"] = text_value(row, "gm");
        item["clearance_style"] = text_value(row, "qcs");
        item["capital_pct"] = number_or_null(number_value(row, "zb"));
        item["actor"] = text_value(row, "zcr");
        item["method"] = text_value(row, "zcfs");
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "announcement_date") >
                   text_value(right, "announcement_date");
        });
    return result;
}

Json normalize_ownership_ranking_rows(
    const Json& rows, const std::string& direction, const std::string& metric,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("ownership ranking rows must be an array");
    if (direction != "increase" && direction != "decrease")
        throw Error("ownership ranking direction must be increase or decrease");
    if (metric != "ratio" && metric != "value" && metric != "count")
        throw Error("ownership ranking metric must be ratio, value, or count");
    const bool decrease = direction == "decrease";
    Json result = Json::array();
    std::uint64_t rank = 0;
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 6)) continue;
        const auto id = row_market_id(row);
        if (!id) continue;
        Json item = Json::object();
        item["rank"] = ++rank;
        item["category"] = direction + "-" + metric;
        item["direction"] = direction;
        item["direction_label"] = decrease ? "减持" : "增持";
        item["metric"] = metric;
        item["security"] = security_document(*id, code, securities);
        item["start_date"] = text_value(row, "date1");
        item["end_date"] = text_value(row, "date2");
        item["increase_shares"] = scaled_number(row, "zcgs", 10000.0);
        item["decrease_shares"] = scaled_number(row, "jcgs", 10000.0);
        item["net_change_shares"] = scaled_number(row, "jzcgs", 10000.0);
        item["increase_value_yuan"] = scaled_number(row, "zcsz", 10000.0);
        item["decrease_value_yuan"] = scaled_number(row, "jcsz", 10000.0);
        item["net_change_value_yuan"] = scaled_number(row, "jzcsz", 10000.0);
        item["increase_count"] = number_or_null(number_value(row, "zccs"));
        item["decrease_count"] = number_or_null(number_value(row, "jccs"));
        const auto increase_count = number_value(row, "zccs").value_or(0.0);
        const auto decrease_count = number_value(row, "jccs").value_or(0.0);
        item["net_change_count"] = increase_count - decrease_count;
        item["ranking_count_magnitude"] =
            number_or_null(number_value(row, "jzccs"));
        const auto ratio = number_value(row, "jzczb");
        item["ranking_float_pct"] = number_or_null(ratio);
        item["signed_float_change_pct"] = ratio
            ? Json((decrease ? -1.0 : 1.0) * std::abs(*ratio)) : Json(nullptr);
        item["price_low"] = number_or_null(number_value(row, "price1"));
        item["price_high"] = number_or_null(number_value(row, "price2"));
        item["raw"] = row;
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_shareholder_count_rows(
    const Json& rows, const std::string& board,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("shareholder count rows must be an array");
    const std::set<std::string> boards{
        "sh-main", "sz-main", "chinext", "star", "bj"};
    if (!boards.count(board)) throw Error("unsupported shareholder-count board");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 6)) continue;
        const auto id = row_market_id(row);
        if (!id) continue;
        Json item = Json::object();
        item["board"] = board;
        item["security"] = security_document(*id, code, securities);
        item["start_date"] = text_value(row, "date1");
        item["end_date"] = text_value(row, "date");
        item["period_days"] = number_or_null(number_value(row, "date3"));
        item["shareholder_households"] = number_or_null(number_value(row, "gdrs1"));
        item["household_change"] = number_or_null(number_value(row, "gdrs2"));
        item["household_change_pct"] = number_or_null(number_value(row, "gdrs3"));
        const auto change_pct = number_value(row, "gdrs3");
        const auto days = number_value(row, "date3");
        item["daily_household_change_pct"] =
            change_pct && days && *days != 0.0
                ? Json(*change_pct / *days) : Json(nullptr);
        item["per_capita_float_shares"] =
            number_or_null(number_value(row, "gdrs4"));
        item["capital_date"] = text_value(row, "jzr1");
        item["top10_report_date"] = text_value(row, "jzr2");
        item["top10_float_shares"] = scaled_number(row, "sdlt1", 10000.0);
        item["top10_float_share_pct"] = number_or_null(number_value(row, "sdlt2"));
        item["top10_shares"] = scaled_number(row, "sdgd1", 10000.0);
        item["top10_total_share_pct"] = number_or_null(number_value(row, "sdgd2"));
        item["institution_report_date"] = text_value(row, "bgq");
        item["institution_shares"] = scaled_number(row, "jgcc1", 10000.0);
        item["institution_float_share_pct"] =
            number_or_null(number_value(row, "jgcc2"));
        item["raw"] = row;
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_insider_change_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities,
    bool with_security) {
    if (!rows.is_array()) throw Error("insider change rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        if (with_security) {
            const auto code = text_value(row, "$ZQDM");
            if (!digits(code, 6)) continue;
            const auto id = row_market_id(row);
            if (!id) continue;
            item["security"] = security_document(*id, code, securities);
        }
        const auto shares = number_value(row, "bdgs");
        const bool decrease = shares && *shares < 0.0;
        item["direction"] = decrease ? "decrease" : "increase";
        item["direction_label"] = decrease ? "减持" : "增持";
        item["date"] = text_value(row, "date");
        item["actor"] = text_value(row, "bdr");
        item["average_price"] = number_or_null(number_value(row, "cjjj"));
        item["close"] = number_or_null(number_value(row, "spj"));
        // Unlike ZCJC, CGGG already uses shares and yuan as its base units.
        item["change_shares"] = number_or_null(shares);
        item["change_amount_yuan"] = number_or_null(number_value(row, "bdje"));
        item["reason"] = text_value(row, "bdyy");
        item["holding_after_shares"] = number_or_null(number_value(row, "bdhcg"));
        item["share_class"] = text_value(row, "zl");
        item["related_person"] = text_value(row, "xm");
        item["position"] = text_value(row, "zw");
        item["relationship"] = text_value(row, "gx");
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "date") > text_value(right, "date");
        });
    return result;
}

Json normalize_no_reduction_commitment_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("commitment rows must be an array");
    const auto today = today_text();
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 6)) continue;
        const auto id = row_market_id(row);
        if (!id) continue;
        const auto start = text_value(row, "qsrq");
        const auto end = text_value(row, "jzrq");
        std::string status = "unknown";
        std::string status_label = "日期未知";
        if (digits(start, 8) && today < start) {
            status = "upcoming";
            status_label = "即将生效";
        } else if (digits(end, 8) && today > end) {
            status = "expired";
            status_label = "已到期";
        } else if (digits(start, 8) || digits(end, 8)) {
            status = "active";
            status_label = "承诺期内";
        }
        const auto detail = normalized_multiline(text_value(row, "xq"));
        Json item = Json::object();
        item["security"] = security_document(*id, code, securities);
        item["announcement_date"] = text_value(row, "ggrq");
        item["announcement_close"] = number_or_null(number_value(row, "price"));
        item["start_date"] = start;
        item["end_date"] = end;
        item["actor"] = labeled_line(detail, "股东名称：");
        item["identity"] = text_value(row, "cnr");
        item["method"] = labeled_line(detail, "变动方式：");
        item["purpose"] = labeled_line(detail, "变动目的：");
        item["transaction_method"] = labeled_line(detail, "交易方式：");
        item["period_note"] = labeled_line(detail, "变动期间说明：");
        item["detail"] = detail;
        item["status"] = status;
        item["status_label"] = status_label;
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