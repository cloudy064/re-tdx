#include "unlocks_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <utility>

namespace tdx {

using namespace unlocks_detail;

Json normalize_unlock_master_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("unlock master rows must be an array");
    std::map<std::string, Json> grouped;
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM1");
        const auto date = text_value(row, "date");
        const auto detail_id = text_value(row, "$ZQDM");
        if (!six_digits(code) || !eight_digits(date) ||
            detail_id != date + code) continue;
        const int market_id = canonical_market_id(text_value(row, "$SC1"));
        const auto shares = number_value(row, "jjsl");
        const auto close = number_value(row, "spj");
        const auto reason = text_value(row, "yy");
        const auto progress = text_value(row, "jjjd");

        Json lot = Json::object();
        lot["unlock_shares"] = number_or_null(shares);
        lot["progress"] = progress;
        lot["reason"] = reason;
        lot["lock_months"] = number_or_null(number_value(row, "sdq"));
        lot["issue_price"] = number_or_null(number_value(row, "fxj"));
        lot["lock_return_pct"] = number_or_null(number_value(row, "sdqsy"));
        lot["pre_month_return_pct"] = number_or_null(number_value(row, "qsy"));
        lot["post_month_return_pct"] = number_or_null(number_value(row, "hsy"));

        auto found = grouped.find(detail_id);
        if (found == grouped.end()) {
            Json event = Json::object();
            event["detail_id"] = detail_id;
            event["date"] = date;
            event["security"] = security_document(market_id, code, securities);
            event["progress"] = progress;
            event["reason"] = reason;
            event["progresses"] = Json::array();
            event["reasons"] = Json::array();
            append_unique_text(event["progresses"], progress);
            append_unique_text(event["reasons"], reason);
            event["mixed_progress"] = false;
            event["mixed_reason"] = false;
            event["unlock_shares"] = shares.value_or(0.0);
            event["pre_unlock_close"] = number_or_null(close);
            event["unlock_market_value"] = shares && close
                ? Json(*shares * *close) : Json(nullptr);
            event["lots"] = Json::array();
            event["lots"].push_back(std::move(lot));
            event["lot_count"] = static_cast<std::uint64_t>(1);
            grouped.emplace(detail_id, std::move(event));
        } else {
            auto& event = found->second;
            if (text_value(event, "date") != date ||
                text_value(event.at("security"), "code") != code ||
                static_cast<int>(event.at("security").at("market_id").as_number()) != market_id)
                throw Error("unlock rows sharing a detail key disagree on event identity");
            append_unique_text(event["progresses"], progress);
            append_unique_text(event["reasons"], reason);
            const auto progress_count = event.at("progresses").size();
            const auto reason_count = event.at("reasons").size();
            event["mixed_progress"] = progress_count > 1;
            event["mixed_reason"] = reason_count > 1;
            event["progress"] = progress_count == 0 ? "" : progress_count == 1
                ? event.at("progresses").as_array().front().as_string()
                : "混合状态";
            event["reason"] = reason_count == 0 ? "" : reason_count == 1
                ? event.at("reasons").as_array().front().as_string()
                : "多种原因";
            event["unlock_shares"] =
                event.at("unlock_shares").as_number() + shares.value_or(0.0);
            if (close && event.at("unlock_market_value").is_number())
                event["unlock_market_value"] =
                    event.at("unlock_market_value").as_number() + shares.value_or(0.0) * *close;
            else event["unlock_market_value"] = Json(nullptr);
            event["lots"].push_back(std::move(lot));
            event["lot_count"] = static_cast<std::uint64_t>(event.at("lots").size());
        }
    }
    Json result = Json::array();
    for (auto& [key, event] : grouped) result.push_back(std::move(event));
    // Within each date, put the largest unlocks first.
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            const auto left_date = text_value(left, "date");
            const auto right_date = text_value(right, "date");
            if (left_date != right_date) return left_date < right_date;
            return left.at("unlock_shares").as_number() >
                   right.at("unlock_shares").as_number();
        });
    return result;
}

Json normalize_unlock_shareholder_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("unlock shareholder rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!six_digits(code)) continue;
        const int market_id = canonical_market_id(text_value(row, "$SC"));
        const auto shares = number_value(row, "jjsl");
        const auto close = number_value(row, "spj");
        Json item = Json::object();
        item["security"] = security_document(market_id, code, securities);
        item["shareholder"] = text_value(row, "xm");
        item["progress"] = text_value(row, "jd");
        item["reason"] = text_value(row, "jjyy");
        item["unlock_shares"] = number_or_null(shares);
        item["pre_unlock_close"] = number_or_null(close);
        item["unlock_market_value"] = shares && close
            ? Json(*shares * *close) : Json(nullptr);
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return number_value(left, "unlock_shares").value_or(0.0) >
                   number_value(right, "unlock_shares").value_or(0.0);
        });
    return result;
}

Json normalize_recent_large_unlock_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("recent large-unlock rows must be an array");
    Json result = Json::array();
    const auto today = today_text();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        const auto date = text_value(row, "jjrq");
        if (!six_digits(code) || !eight_digits(date)) continue;
        const int market_id = canonical_market_id(text_value(row, "$SC"));
        const auto shares = number_value(row, "jjsl");
        const auto ratio = number_value(row, "jjgzb");
        const auto total = number_value(row, "zgb");
        Json event = Json::object();
        event["detail_id"] = date + code;
        event["date"] = date;
        event["security"] = security_document(market_id, code, securities);
        event["progress"] = date < today ? "已解禁" : date == today ? "今日解禁" : "待解禁";
        event["reason"] = text_value(row, "jjyy");
        event["unlock_shares"] = shares.value_or(0.0);
        event["pre_unlock_close"] = Json(nullptr);
        event["unlock_market_value"] = Json(nullptr);
        event["unlock_to_total_ratio"] = number_or_null(ratio);
        event["unlock_to_total_pct"] = ratio ? Json(*ratio * 100.0) : Json(nullptr);
        event["total_shares"] = number_or_null(total);
        event["lot_count"] = static_cast<std::uint64_t>(0);
        event["lots"] = Json::array();
        event["source_kind"] = "recent-large-window";
        event["raw"] = row;
        result.push_back(std::move(event));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            const auto left_date = text_value(left, "date");
            const auto right_date = text_value(right, "date");
            if (left_date != right_date) return left_date > right_date;
            const auto left_ratio = number_value(left, "unlock_to_total_ratio").value_or(0.0);
            const auto right_ratio = number_value(right, "unlock_to_total_ratio").value_or(0.0);
            return left_ratio > right_ratio;
        });
    return result;
}

Json normalize_monthly_unlock_pressure_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("monthly unlock-pressure rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto month = text_value(row, "$ZQDM");
        if (month.size() != 6 || !std::all_of(month.begin(), month.end(),
                [](char ch) { return ch >= '0' && ch <= '9'; })) continue;
        const auto shares_100m = number_value(row, "jjsl");
        const auto value_100m = number_value(row, "jjsz");
        const auto total_cap = number_value(row, "J_ZSZ");
        const auto float_cap = number_value(row, "J_LTSZ");
        Json item = Json::object();
        item["month"] = month;
        item["unlock_shares_100m"] = number_or_null(shares_100m);
        item["unlock_shares"] = shares_100m ? Json(*shares_100m * 100000000.0) : Json(nullptr);
        item["unlock_market_value_100m_yuan"] = number_or_null(value_100m);
        item["unlock_market_value_yuan"] = value_100m
            ? Json(*value_100m * 100000000.0) : Json(nullptr);
        item["total_market_cap_yuan"] = number_or_null(total_cap);
        item["float_market_cap_yuan"] = number_or_null(float_cap);
        item["unlock_to_total_market_cap_pct"] = value_100m && total_cap && *total_cap
            ? Json(*value_100m * 100000000.0 * 100.0 / *total_cap) : Json(nullptr);
        item["unlock_to_float_market_cap_pct"] = value_100m && float_cap && *float_cap
            ? Json(*value_100m * 100000000.0 * 100.0 / *float_cap) : Json(nullptr);
        item["security_count"] = number_or_null(number_value(row, "jjgps"));
        item["lot_count"] = number_or_null(number_value(row, "jjts"));
        item["source_resource"] = monthly_pressure_resource;
        item["raw"] = row;
        result.push_back(std::move(item));
    }
    std::sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "month") < text_value(right, "month");
        });
    return result;
}

}  // namespace tdx
