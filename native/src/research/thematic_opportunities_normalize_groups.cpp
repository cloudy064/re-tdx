#include "thematic_opportunities_internal.hpp"

#include "tdx/common.hpp"

#include <set>

namespace tdx {

Json normalize_opportunity_groups(
    const Json& rows, const std::string& type_value,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace detail::thematic_opportunities;
    if (!rows.is_array()) throw Error("opportunity-group master rows must be an array");
    const auto type = lower_ascii(trim(type_value));
    const auto& definition = resource_for_group_type(type);
    const auto declared_page = std::string(legacy_declared_page_name());
    Json result = Json::array();
    std::set<std::string> ids;
    for (const auto& raw : rows.as_array()) {
        const auto id = text_value(raw, "$ZQDM");
        const auto name = text_value(raw, definition.name_field);
        if (id.empty() || name.empty() || !ids.insert(id).second)
            throw Error("opportunity-group id/name is missing or duplicated");
        std::size_t raw_count = 0;
        const auto parsed = parse_members(text_value(raw, "$S_ZQDM"), &raw_count);
        Json members = Json::array();
        for (const auto& [market, code] : parsed)
            members.push_back(security_document(market, code, securities));
        Json group = Json::object();
        group["group_id"] = id;
        group["type"] = type;
        group["type_name"] = std::string(definition.type_name);
        group["name"] = name;
        group["category"] = text_value(raw, definition.category_field);
        group["raw_member_count"] = static_cast<std::uint64_t>(raw_count);
        group["member_count"] = static_cast<std::uint64_t>(parsed.size());
        group["duplicate_member_count"] =
            static_cast<std::uint64_t>(raw_count - parsed.size());
        group["detail_resource"] = std::string(definition.detail_prefix) + id + ".jsn";
        group["source_resource"] = std::string(definition.resource);
        group["page_declared_name"] = definition.legacy
            ? Json(declared_page) : Json(nullptr);
        group["semantic_mismatch"] = definition.legacy && name != declared_page;
        group["semantic_note"] = definition.legacy && name != declared_page
            ? "客户端页面声明为“虚拟现实”，但当前上游分组实际为“" + name +
                "”；以行内业务名称为准，不继承页面标签。"
            : "";
        group["members"] = std::move(members);
        group["raw"] = raw;
        result.push_back(std::move(group));
    }
    return result;
}

Json normalize_opportunity_group_details(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace detail::thematic_opportunities;
    if (!rows.is_array()) throw Error("opportunity-group detail rows must be an array");
    Json result = Json::array();
    std::set<std::pair<int, std::string>> seen;
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        if (!digits(code, 6)) continue;
        int market = -1;
        try { market = market_id(text_value(raw, "$SC")); } catch (...) { continue; }
        if (!seen.insert({market, code}).second)
            throw Error("duplicate security in opportunity-group detail: " + code);
        Json row = Json::object();
        row["security"] = security_document(market, code, securities);
        row["logic"] = text_value(raw, "TZLJ");
        row["description"] = text_value(raw, "XXSM");
        row["reference_close_3d"] = Json(nullptr);
        row["reference_close_5d"] = Json(nullptr);
        row["reference_close_20d"] = Json(nullptr);
        row["three_month_adjusted_close"] = number_or_null(number_value(raw, "JSYSP"));
        row["year_start_adjusted_close"] = number_or_null(number_value(raw, "NZJSP"));
        row["limit_up_count"] = number_or_null(number_value(raw, "lzcs"));
        row["detail_variant"] = "ydyl-opportunity";
        row["source_resource"] = "ydyl1";
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

Json normalize_legacy_client_theme_details(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace detail::thematic_opportunities;
    if (!rows.is_array())
        throw Error("legacy client-theme detail rows must be an array");
    Json result = Json::array();
    std::set<std::pair<int, std::string>> seen;
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        if (!digits(code, 6)) continue;
        int market = -1;
        try { market = market_id(text_value(raw, "$SC")); } catch (...) { continue; }
        if (!seen.insert({market, code}).second)
            throw Error("duplicate security in legacy client-theme detail: " + code);
        Json row = Json::object();
        row["security"] = security_document(market, code, securities);
        row["logic"] = text_value(raw, "kd");
        auto description = text_value(raw, "xq");
        if (description.empty()) description = text_value(raw, "T005");
        row["description"] = description;
        row["reference_close_3d"] = number_or_null(number_value(raw, "fqprice_d3"));
        row["reference_close_5d"] = number_or_null(number_value(raw, "fqprice_d5"));
        row["reference_close_20d"] = number_or_null(number_value(raw, "fqprice_d20"));
        row["three_month_adjusted_close"] = number_or_null(number_value(raw, "price"));
        row["year_start_adjusted_close"] = Json(nullptr);
        row["limit_up_count"] = number_or_null(number_value(raw, "lzcs"));
        row["detail_variant"] = "legacy-client-theme";
        row["source_resource"] = "xnxs";
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace tdx
