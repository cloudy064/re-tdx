#include "tdx/theme_library_internal.hpp"

#include "tdx/common.hpp"

#include <set>
#include <utility>

namespace tdx {

using namespace tdx::theme_library_detail;

Json normalize_theme_library_rows(
    const Json& rows, const ThemeLibrarySource& source,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("theme-library rows must be an array");
    Json result = Json::array();
    std::set<std::string> ids;
    for (const auto& raw : rows.as_array()) {
        const auto id = text_value(raw, "$ZQDM");
        const auto name = text_value(raw, "name");
        if (id.empty() || name.empty() || !ids.insert(id).second)
            throw Error("theme-library id/name is missing or duplicated in " + source.resource);
        std::size_t raw_count = 0;
        const auto parsed = parse_members(text_value(raw, "$S_ZQDM"), &raw_count);
        Json members = Json::array();
        for (const auto& [market, code] : parsed)
            members.push_back(security_document(market, code, securities));
        const auto declared = unsigned_value(raw, "gpsl");
        const auto age_days = unsigned_value(raw, "zzts");
        const auto limit_up = unsigned_value(raw, "ztgs");
        const auto limit_down = unsigned_value(raw, "dtgs");
        const auto broken_limit = unsigned_value(raw, "zbs");
        Json row = Json::object();
        row["record_id"] = source.id + ":" + id;
        row["theme_id"] = id;
        row["source"] = source.id;
        row["source_name"] = source.name;
        row["name"] = name;
        row["type"] = text_value(raw, "zttype");
        row["description"] = text_value(raw, "ztms");
        row["created_date"] = text_value(raw, "cjrq");
        row["age_days"] = age_days ? Json(*age_days) : Json(nullptr);
        row["declared_member_count"] = declared
            ? Json(*declared) : Json(static_cast<std::uint64_t>(raw_count));
        row["raw_member_count"] = static_cast<std::uint64_t>(raw_count);
        row["member_count"] = static_cast<std::uint64_t>(parsed.size());
        row["duplicate_member_count"] = static_cast<std::uint64_t>(raw_count - parsed.size());
        row["count_matches_raw"] = !declared || *declared == raw_count;
        row["limit_up_count"] = limit_up ? Json(*limit_up) : Json(nullptr);
        row["limit_down_count"] = limit_down ? Json(*limit_down) : Json(nullptr);
        row["broken_limit_count"] = broken_limit ? Json(*broken_limit) : Json(nullptr);
        row["detail_resource"] = "zttz/" + id + ".jsn";
        row["chart_resource"] = "zttz1/" + id + ".jsn";
        row["members"] = std::move(members);
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

Json normalize_theme_library_details(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("theme-library detail rows must be an array");
    Json result = Json::array();
    std::set<std::pair<int, std::string>> seen;
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        if (!digits(code, 6)) continue;
        int market = -1;
        try { market = market_id(text_value(raw, "$SC")); } catch (...) { continue; }
        if (!seen.insert({market, code}).second)
            throw Error("duplicate security in theme-library detail: " + code);
        Json prices = Json::object();
        for (const auto& [field, key] : std::vector<std::pair<std::string, std::string>>{
                 {"fqprice_d3", "3d"}, {"fqprice_d5", "5d"},
                 {"fqprice_d20", "20d"}, {"fqprice_d60", "60d"},
                 {"FQPrice_M3", "3m"}}) {
            const auto value = number_value(raw, field);
            prices[key] = value ? Json(*value) : Json(nullptr);
        }
        const auto leader_count = unsigned_value(raw, "lzcs");
        Json row = Json::object();
        row["security"] = security_document(market, code, securities);
        row["reference_prices"] = std::move(prices);
        row["leader_count"] = leader_count ? Json(*leader_count) : Json(nullptr);
        row["relation_strength"] = text_value(raw, "glcd");
        row["description"] = text_value(raw, "Contents");
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

Json normalize_theme_library_chart(const Json& rows) {
    if (!rows.is_array()) throw Error("theme-library chart rows must be an array");
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto date = text_value(raw, "date");
        const auto value = number_value(raw, "jgzst");
        if (date.empty() || !value) continue;
        Json point = Json::object();
        point["date"] = date;
        point["value"] = *value;
        result.push_back(std::move(point));
    }
    return result;
}

}  // namespace tdx
