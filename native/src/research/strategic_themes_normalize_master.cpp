#include "strategic_themes_internal.hpp"

#include <set>

namespace tdx {

Json normalize_strategic_theme_master(
    const Json& rows, const StrategicThemeCategory& category,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace detail::strategic_themes;
    if (!rows.is_array()) throw Error("strategic theme master rows must be an array");
    Json result = Json::array();
    std::set<std::string> ids;
    for (const auto& raw : rows.as_array()) {
        const auto source_id = text_value(raw, "$ZQDM");
        const auto id = category.id_prefix + source_id;
        const auto name = text_value(raw, category.name_field);
        if (source_id.empty() || name.empty() || !ids.insert(id).second)
            throw Error("strategic theme id/name is missing or duplicated in " +
                        category.resource);
        std::size_t raw_count = 0;
        const auto parsed = parse_members(text_value(raw, "$S_ZQDM"), &raw_count);
        const auto declared = unsigned_value(raw, "S_NUM");
        Json members = Json::array();
        for (const auto& [market, code] : parsed)
            members.push_back(security_document(market, code, securities));
        Json row = Json::object();
        row["theme_id"] = id;
        row["source_theme_id"] = source_id;
        row["name"] = name;
        row["description"] = category.description_field.empty()
            ? Json("") : Json(text_value(raw, category.description_field));
        row["categories"] = Json::array();
        row["categories"].push_back(category.name);
        row["category_block_ids"] = Json::array();
        row["category_block_ids"].push_back(category.block_id);
        row["master_declared_member_count"] = declared
            ? Json(*declared) : Json(static_cast<std::uint64_t>(raw_count));
        row["declared_member_count_available"] = declared.has_value();
        row["master_raw_member_count"] = static_cast<std::uint64_t>(raw_count);
        row["master_member_count"] = static_cast<std::uint64_t>(parsed.size());
        row["master_duplicate_member_count"] =
            static_cast<std::uint64_t>(raw_count - parsed.size());
        row["count_matches_raw"] = !declared || *declared == raw_count;
        row["detail_resource"] = "zttzty/" + source_id + ".jsn";
        row["members"] = std::move(members);
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace tdx
