#include "research_internal.hpp"

#include <algorithm>

namespace tdx {

using namespace detail::research;

Json normalize_research_activity_rows(const Json& rows, bool include_text) {
    if (!rows.is_array()) throw Error("research activity rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto [headline, body] = split_embedded_text(text_value(row, "title"));
        Json item = Json::object();
        item["date"] = copy_value(row, "date");
        item["title"] = headline;
        item["text_length"] = static_cast<std::uint64_t>(body.size());
        if (include_text) item["text"] = body;
        item["source_urls"] = urls_json(body);
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "date") > text_value(right, "date");
        });
    return result;
}

Json normalize_research_regulatory_rows(const Json& rows, bool include_text) {
    if (!rows.is_array()) throw Error("research regulatory rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto raw = text_value(row, "sjjs");
        const auto [summary, attachment] = split_embedded_text(raw);
        Json item = Json::object();
        item["date"] = copy_value(row, "sjrq");
        item["subject"] = copy_value(row, "sjry");
        item["event_type"] = copy_value(row, "sjlx");
        item["progress"] = copy_value(row, "sjjz");
        item["summary_length"] = static_cast<std::uint64_t>(summary.size());
        if (include_text) item["summary"] = summary;
        item["source_urls"] = urls_json(attachment.empty() ? raw : attachment);
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "date") > text_value(right, "date");
        });
    return result;
}

Json normalize_research_security_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("research drill-down rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!six_digits(code)) throw Error("research drill-down security code is invalid");
        const int market_id = canonical_market_id(text_value(row, "$SC"));
        Json item = Json::object();
        item["security"] = security_document(market_id, code, securities);
        item["latest_date"] = copy_value(row, "date");
        Json data = Json::object();
        map_fields(data, row, {
            {"return_pct_1m", "zf1"}, {"return_pct_3m", "zf2"},
            {"return_pct_6m", "zf3"}, {"return_pct_1m", "QuarterPrice"},
            {"return_pct_3m", "HarfYearPrice"}, {"return_pct_6m", "YearPrice"},
            {"research_count_1m", "sydycs"}, {"research_count_3m", "bndycs"},
            {"research_count_6m", "yndycs"}, {"institution_count_1m", "syjgsl"},
            {"institution_count_3m", "bnjgsl"}, {"institution_count_6m", "ynjgsl"}
        });
        item["data"] = std::move(data);
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "latest_date") > text_value(right, "latest_date");
        });
    return result;
}

}  // namespace tdx
