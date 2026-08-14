#include "convertible_bonds_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/market.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {

using namespace convertible_bond_detail;

Json normalize_pending_convertible_bond_document(
    const Json& document,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!document.is_object())
        throw Error("pending convertible-bond normalization requires one JSN document");
    const auto rows_field = document.as_object().find("rows");
    if (rows_field == document.as_object().end() || !rows_field->second.is_array())
        throw Error("pending convertible-bond normalization requires one JSN document");
    Json rows = Json::array();
    for (const auto& row : document.at("rows").as_array()) {
        const auto code = value_text(row, "$ZQDM");
        if (!valid_code(code)) continue;
        int market = -1;
        try { market = market_id(value_text(row, "$SC")); }
        catch (...) { continue; }
        Json item = Json::object();
        item["underlying"] = security_document(market, code, "", securities);
        item["issue_type"] = value_text(row, "zzlx");
        item["planned_issue_size_100m_yuan"] = number(row, "mzgm");
        item["plan_progress"] = value_text(row, "fadj");
        item["progress_date"] = value_text(row, "date0");
        item["stock_rights_yuan"] = number(row, "byhq");
        item["conversion_price_yuan"] = number(row, "zgj");
        item["shareholder_placement_ratio"] = number(row, "gdpsl");
        item["subscription_date"] = value_text(row, "sgrq");
        item["issue_date"] = value_text(row, "fxrq");
        item["lottery_rate"] = number(row, "zql");
        item["lottery_date"] = value_text(row, "zqr");
        item["subscription_code"] = value_text(row, "sgdm");
        item["subscription_name"] = value_text(row, "sgmc");
        item["issue_price_yuan"] = number(row, "fxjg");
        item["raw"] = row;
        rows.push_back(std::move(item));
    }
    return rows;
}

Json normalize_convertible_bond_subscription_document(
    const Json& document,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!document.is_object() || !document.as_object().count("rows") ||
        !document.at("rows").is_array())
        throw Error("convertible-bond subscription normalization requires one JSN document");
    Json rows = Json::array();
    for (const auto& row : document.at("rows").as_array()) {
        const auto bond_code = value_text(row, "$ZQDM");
        const auto stock_code = value_text(row, "$ZQDM1");
        if (!valid_code(bond_code) || !valid_code(stock_code)) continue;
        int bond_market = -1, stock_market = -1;
        try {
            bond_market = market_id(value_text(row, "$SC"));
            stock_market = market_id(value_text(row, "$SC1"));
        } catch (...) { continue; }
        const auto stock_close = value_number(row, "zxj");
        const auto conversion_price = value_number(row, "zgj");
        const auto bond_close = value_number(row, "zxsp");
        const auto conversion_value = stock_close && conversion_price &&
                *conversion_price != 0.0
            ? std::optional<double>(*stock_close * 100.0 / *conversion_price)
            : std::nullopt;
        const auto premium = bond_close && conversion_value &&
                *conversion_value != 0.0
            ? std::optional<double>((*bond_close - *conversion_value) * 100.0 /
                                    *conversion_value)
            : std::nullopt;
        Json item = Json::object();
        item["kind"] = "convertible-bond-subscription";
        item["bond"] = security_document(bond_market, bond_code,
                                          value_text(row, "ZQJC"), securities);
        item["underlying"] = security_document(stock_market, stock_code, "", securities);
        item["subscription_date"] = value_text(row, "sgrq");
        item["subscription_code"] = value_text(row, "sgdm");
        item["subscription_limit_10k_yuan"] = number(row, "sgsx");
        item["conversion_start_date"] = value_text(row, "zgr");
        item["underlying_close_yuan"] = optional_number(stock_close);
        item["conversion_price_yuan"] = optional_number(conversion_price);
        item["conversion_value_yuan"] = optional_number(conversion_value);
        item["bond_close_yuan"] = optional_number(bond_close);
        item["conversion_premium_pct"] = optional_number(premium);
        item["issue_size_100m_yuan"] = number(row, "fxzs");
        item["lottery_date"] = value_text(row, "zqr");
        item["lottery_rate_pct"] = number(row, "zql");
        item["listing_date"] = value_text(row, "ssrq");
        item["listed"] = !value_text(row, "ssrq").empty();
        item["event_id"] = "convertible-subscription:" +
            std::to_string(bond_market) + ":" + bond_code + ":" +
            value_text(row, "sgrq");
        item["source_resource"] = subscription_resource;
        item["raw"] = row;
        rows.push_back(std::move(item));
    }
    return rows;
}

Json normalize_new_convertible_bond_projection_document(
    const Json& document,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!document.is_object() || !document.as_object().count("rows") ||
        !document.at("rows").is_array())
        throw Error("new convertible-bond projection normalization requires one JSN document");
    Json rows = Json::array();
    for (const auto& row : document.at("rows").as_array()) {
        const auto stock_code = value_text(row, "$ZQDM");
        if (!valid_code(stock_code)) continue;
        int stock_market = -1;
        try { stock_market = market_id(value_text(row, "$SC")); }
        catch (...) { continue; }
        auto subscription_date = compact_date_prefix(value_text(row, "sgrq"));
        if (subscription_date.empty())
            subscription_date = compact_date_prefix(value_text(row, "fxrq"));
        Json item = Json::object();
        item["kind"] = "new-convertible-bond-projection";
        item["event_id"] = "convertible-new-projection:" +
            std::to_string(stock_market) + ":" + stock_code + ":" +
            value_text(row, "sgdm");
        item["underlying"] = security_document(
            stock_market, stock_code, "", securities);
        item["subscription_code"] = value_text(row, "sgdm");
        item["subscription_name"] = value_text(row, "sgmc");
        item["subscription_date"] = subscription_date;
        item["subscription_date_text"] = value_text(row, "sgrq");
        item["issue_date"] = compact_date_prefix(value_text(row, "fxrq"));
        item["issue_price_yuan"] = number(row, "fxjg");
        item["issue_type"] = value_text(row, "zzlx");
        item["issue_size_100m_yuan"] = number(row, "mzgm");
        item["stock_rights_yuan"] = number(row, "byhq");
        item["conversion_price_yuan"] = number(row, "zgj");
        item["conversion_available"] = value_text(row, "zg");
        item["conversion_start_date"] = value_text(row, "zgqsr");
        item["conversion_end_date"] = value_text(row, "zgjzr");
        item["shareholder_placement_ratio"] = number(row, "gdpsl");
        item["lottery_date"] = value_text(row, "zqr");
        item["lottery_rate_pct"] = number(row, "zql");
        item["region"] = value_text(row, "ssdy");
        item["plan_progress"] = value_text(row, "fadj");
        item["progress_date"] = value_text(row, "date0");
        item["source_resource"] = new_bond_projection_resource;
        item["raw"] = row;
        rows.push_back(std::move(item));
    }
    return rows;
}

Json reconcile_new_convertible_bond_projection(
    const Json& subscription_rows, const Json& projection_rows) {
    if (!subscription_rows.is_array() || !projection_rows.is_array())
        throw Error("convertible-bond projection reconciliation requires two arrays");
    std::map<std::string, std::vector<const Json*>> by_subscription_code;
    std::map<std::string, std::vector<const Json*>> by_underlying;
    for (const auto& row : subscription_rows.as_array()) {
        const auto code = row.at("subscription_code").as_string();
        if (!code.empty()) by_subscription_code[code].push_back(&row);
        const auto id = row.at("underlying").at("security_id").as_string();
        if (!id.empty()) by_underlying[id].push_back(&row);
    }
    Json enriched = Json::array();
    Json unmatched_codes = Json::array();
    std::uint64_t exact_code_matches = 0, underlying_only_matches = 0;
    std::uint64_t issue_size_mismatches = 0, subscription_date_mismatches = 0;
    std::uint64_t hybrid_or_stale = 0;
    for (const auto& source : projection_rows.as_array()) {
        auto row = source;
        const auto code = row.at("subscription_code").as_string();
        const auto underlying = row.at("underlying").at("security_id").as_string();
        const Json* matched = nullptr;
        const auto exact = by_subscription_code.find(code);
        if (!code.empty() && exact != by_subscription_code.end()) {
            const auto same_underlying = std::find_if(
                exact->second.begin(), exact->second.end(),
                [&](const Json* candidate) {
                    return candidate->at("underlying").at("security_id").as_string() ==
                        underlying;
                });
            matched = same_underlying == exact->second.end()
                ? exact->second.front() : *same_underlying;
            ++exact_code_matches;
        } else {
            const auto fallback = by_underlying.find(underlying);
            if (fallback != by_underlying.end() && !fallback->second.empty()) {
                matched = fallback->second.front();
                ++underlying_only_matches;
            }
        }
        Json match = Json::object();
        match["matched"] = matched != nullptr;
        match["match_method"] = exact != by_subscription_code.end()
            ? "subscription-code" : matched ? "underlying-only" : "none";
        bool date_mismatch = false, size_mismatch = false;
        if (matched) {
            match["event_id"] = matched->at("event_id");
            match["bond"] = matched->at("bond");
            match["primary_subscription_date"] = matched->at("subscription_date");
            match["primary_issue_size_100m_yuan"] = matched->at("issue_size_100m_yuan");
            const auto projection_date = row.at("subscription_date").as_string();
            const auto primary_date = matched->at("subscription_date").as_string();
            date_mismatch = !projection_date.empty() && !primary_date.empty() &&
                projection_date != primary_date;
            match["subscription_date_mismatch"] = date_mismatch;
            if (row.at("issue_size_100m_yuan").is_number() &&
                matched->at("issue_size_100m_yuan").is_number()) {
                const auto delta = row.at("issue_size_100m_yuan").as_number() -
                    matched->at("issue_size_100m_yuan").as_number();
                match["issue_size_delta_100m_yuan"] = delta;
                size_mismatch = std::abs(delta) > 0.01;
            } else match["issue_size_delta_100m_yuan"] = Json(nullptr);
            match["issue_size_mismatch"] = size_mismatch;
        } else {
            match["event_id"] = "";
            match["bond"] = Json(nullptr);
            match["primary_subscription_date"] = "";
            match["primary_issue_size_100m_yuan"] = Json(nullptr);
            match["subscription_date_mismatch"] = false;
            match["issue_size_delta_100m_yuan"] = Json(nullptr);
            match["issue_size_mismatch"] = false;
            unmatched_codes.push_back(code);
        }
        if (date_mismatch) ++subscription_date_mismatches;
        if (size_mismatch) ++issue_size_mismatches;
        if (date_mismatch || size_mismatch) ++hybrid_or_stale;
        row["subscription_match"] = std::move(match);
        enriched.push_back(std::move(row));
    }
    Json summary = Json::object();
    summary["primary_resource"] = subscription_resource;
    summary["projection_resource"] = new_bond_projection_resource;
    summary["primary_rows"] = static_cast<std::uint64_t>(subscription_rows.size());
    summary["projection_rows"] = static_cast<std::uint64_t>(projection_rows.size());
    summary["exact_subscription_code_matches"] = exact_code_matches;
    summary["underlying_only_matches"] = underlying_only_matches;
    summary["unmatched_projection_rows"] = static_cast<std::uint64_t>(
        projection_rows.size()) - exact_code_matches - underlying_only_matches;
    summary["issue_size_mismatch_count"] = issue_size_mismatches;
    summary["subscription_date_mismatch_count"] = subscription_date_mismatches;
    summary["hybrid_or_stale_count"] = hybrid_or_stale;
    summary["unmatched_subscription_codes"] = std::move(unmatched_codes);
    summary["exact_projection"] = exact_code_matches == projection_rows.size() &&
        hybrid_or_stale == 0;
    Json result = Json::object();
    result["rows"] = std::move(enriched);
    result["summary"] = std::move(summary);
    return result;
}

void sort_pending_convertible_bond_rows(Json& rows, const std::string& sort_value,
                                        const std::string& order_value) {
    if (!rows.is_array()) throw Error("pending convertible-bond rows must be an array");
    const auto sort = lower_ascii(trim(sort_value.empty() ? "progress-date" : sort_value));
    const auto order = lower_ascii(trim(order_value.empty() ? "desc" : order_value));
    const std::map<std::string, std::string> fields{
        {"progress-date", "progress_date"}, {"issue-size", "planned_issue_size_100m_yuan"},
        {"stock-rights", "stock_rights_yuan"}, {"conversion-price", "conversion_price_yuan"},
        {"subscription-date", "subscription_date"}};
    const auto selected = fields.find(sort);
    if (selected == fields.end())
        throw Error("pending sort must be progress-date, issue-size, stock-rights, conversion-price, or subscription-date");
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    const auto field_name = selected->second;
    const bool descending = order == "desc";
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            const auto& l = left.at(field_name);
            const auto& r = right.at(field_name);
            const bool l_missing = l.is_null() || (l.is_string() && l.as_string().empty());
            const bool r_missing = r.is_null() || (r.is_string() && r.as_string().empty());
            if (l_missing != r_missing) return !l_missing;
            if (l_missing) return false;
            if (l.is_number() && r.is_number())
                return descending ? l.as_number() > r.as_number()
                                  : l.as_number() < r.as_number();
            const auto ls = l.is_string() ? l.as_string() : l.dump(-1);
            const auto rs = r.is_string() ? r.as_string() : r.dump(-1);
            return descending ? ls > rs : ls < rs;
        });
}

void sort_convertible_bond_subscription_rows(Json& rows,
                                             const std::string& sort_value,
                                             const std::string& order_value) {
    if (!rows.is_array()) throw Error("convertible-bond subscription rows must be an array");
    const auto sort = lower_ascii(trim(sort_value.empty() ? "subscription-date" : sort_value));
    const auto order = lower_ascii(trim(order_value.empty() ? "desc" : order_value));
    const std::map<std::string, std::string> fields{
        {"subscription-date", "subscription_date"},
        {"listing-date", "listing_date"},
        {"issue-size", "issue_size_100m_yuan"},
        {"premium", "conversion_premium_pct"},
        {"lottery-rate", "lottery_rate_pct"}};
    const auto selected = fields.find(sort);
    if (selected == fields.end())
        throw Error("subscription sort must be subscription-date, listing-date, issue-size, premium, or lottery-rate");
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    const bool descending = order == "desc";
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            const auto& l = left.at(selected->second);
            const auto& r = right.at(selected->second);
            const bool lm = l.is_null() || (l.is_string() && l.as_string().empty());
            const bool rm = r.is_null() || (r.is_string() && r.as_string().empty());
            if (lm != rm) return !lm;
            if (lm) return left.at("bond").at("code").as_string() <
                right.at("bond").at("code").as_string();
            if (l.is_number() && r.is_number())
                return descending ? l.as_number() > r.as_number()
                                  : l.as_number() < r.as_number();
            return descending ? l.as_string() > r.as_string()
                              : l.as_string() < r.as_string();
        });
}

}  // namespace tdx
