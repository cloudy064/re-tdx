#include "strategic_themes_internal.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <set>

namespace tdx {

StrategicThemeService::FetchResult StrategicThemeService::fetch_master(
    const StrategicThemeQuery& options) {
    using namespace detail::strategic_themes;
    const auto now = std::time(nullptr);
    const int age = master_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(
              0, now - master_cache_.fetched_at))
        : 0;
    if (!options.refresh && master_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {master_cache_.document, false, age};

    std::vector<std::string> resources;
    for (const auto& category : strategic_theme_categories())
        resources.push_back(category.resource);
    const auto documents = fetch_jsn_resources_rows(
        resources, "bi", options.timeout_ms);
    Json categories = Json::array();
    Json sources = Json::array();
    std::map<std::string, Json> themes;
    std::set<std::pair<int, std::string>> stocks;
    std::uint64_t category_memberships = 0;
    std::uint64_t raw_stock_memberships = 0;
    std::uint64_t duplicate_memberships = 0;
    std::uint64_t count_mismatches = 0;
    for (const auto& category : strategic_theme_categories()) {
        const auto& source = document_for_resource(documents, category.resource);
        sources.push_back(jsn_source_metadata(source));
        const auto normalized = normalize_strategic_theme_master(
            source.at("rows"), category, blocks_.securities);
        Json category_row = Json::object();
        category_row["block_id"] = category.block_id;
        category_row["name"] = category.name;
        category_row["config_name"] = category.config_name;
        category_row["resource"] = category.resource;
        category_row["theme_count"] = static_cast<std::uint64_t>(normalized.size());
        Json ids = Json::array();
        for (const auto& theme : normalized.as_array()) {
            ids.push_back(theme.at("theme_id"));
            ++category_memberships;
            raw_stock_memberships += static_cast<std::uint64_t>(
                theme.at("master_raw_member_count").as_number());
            duplicate_memberships += static_cast<std::uint64_t>(
                theme.at("master_duplicate_member_count").as_number());
            if (!theme.at("count_matches_raw").as_bool()) ++count_mismatches;
            for (const auto& member : theme.at("members").as_array())
                stocks.insert(std::make_pair(
                    static_cast<int>(member.at("market_id").as_number()),
                    member.at("code").as_string()));
            const auto id = theme.at("theme_id").as_string();
            const auto found = themes.find(id);
            if (found == themes.end()) themes.emplace(id, theme);
            else {
                if (found->second.at("name").as_string() !=
                        theme.at("name").as_string() ||
                    member_ids(found->second) != member_ids(theme) ||
                    found->second.at("master_declared_member_count").as_number() !=
                        theme.at("master_declared_member_count").as_number())
                    throw Error("strategic theme differs across categories: " + id);
                found->second["categories"].push_back(category.name);
                found->second["category_block_ids"].push_back(category.block_id);
            }
        }
        category_row["theme_ids"] = std::move(ids);
        categories.push_back(std::move(category_row));
    }

    Json theme_rows = Json::array();
    std::uint64_t unique_stock_memberships = 0;
    for (auto& [id, theme] : themes) {
        (void)id;
        unique_stock_memberships += static_cast<std::uint64_t>(
            theme.at("master_member_count").as_number());
        theme_rows.push_back(std::move(theme));
    }
    sort_themes(theme_rows, sort_spec("name"), "asc");
    Json summary = Json::object();
    summary["category_count"] = static_cast<std::uint64_t>(categories.size());
    summary["theme_count"] = static_cast<std::uint64_t>(theme_rows.size());
    summary["category_theme_memberships"] = category_memberships;
    summary["raw_theme_stock_memberships"] = raw_stock_memberships;
    summary["theme_stock_memberships"] = unique_stock_memberships;
    summary["security_count"] = static_cast<std::uint64_t>(stocks.size());
    summary["duplicate_master_memberships"] = duplicate_memberships;
    summary["count_mismatch_count"] = count_mismatches;
    Json document = Json::object();
    document["categories"] = std::move(categories);
    document["themes"] = std::move(theme_rows);
    document["summary"] = std::move(summary);
    document["sources"] = std::move(sources);
    document["upstream_health"] = jsn_sources_health(document.at("sources"));
    master_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

}  // namespace tdx
