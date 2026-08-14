#include "tdx/theme_library_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <ctime>
#include <set>
#include <string>
#include <utility>

namespace fs = std::filesystem;

namespace tdx {

using namespace tdx::theme_library_detail;

ThemeLibraryService::ThemeLibraryService(
    fs::path root, std::map<std::pair<int, std::string>, Security> securities)
    : root_(std::move(root)) {
    blocks_.securities = std::move(securities);
}

// Loads all five masters in one pass and folds them into a single themes array
// plus a cross-source summary. The whole fold is cached as one document because
// the summary counts span sources.
ThemeLibraryService::FetchResult ThemeLibraryService::fetch_master(
    const ThemeLibraryQuery& options) {
    const auto now = std::time(nullptr);
    const int age = master_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - master_cache_.fetched_at)) : 0;
    if (!options.refresh && master_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {master_cache_.document, false, age};
    std::vector<std::string> resources;
    for (const auto& source : theme_library_sources()) resources.push_back(source.resource);
    const auto documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);
    Json themes = Json::array(), source_rows = Json::array(), sources = Json::array();
    std::set<std::string> unique_ids;
    std::set<std::pair<int, std::string>> stocks;
    std::uint64_t memberships = 0, mismatches = 0;
    for (const auto& source : theme_library_sources()) {
        const auto& document = document_for_resource(documents, source.resource);
        sources.push_back(jsn_source_metadata(document));
        auto normalized = normalize_theme_library_rows(
            document.at("rows"), source, blocks_.securities);
        Json source_row = Json::object();
        source_row["id"] = source.id;
        source_row["name"] = source.name;
        source_row["resource"] = source.resource;
        source_row["theme_count"] = static_cast<std::uint64_t>(normalized.size());
        source_rows.push_back(std::move(source_row));
        for (auto& theme : normalized.as_array()) {
            unique_ids.insert(theme.at("theme_id").as_string());
            memberships += static_cast<std::uint64_t>(theme.at("member_count").as_number());
            if (!theme.at("count_matches_raw").as_bool()) ++mismatches;
            for (const auto& member : theme.at("members").as_array())
                stocks.insert({static_cast<int>(member.at("market_id").as_number()),
                               member.at("code").as_string()});
            themes.push_back(std::move(theme));
        }
    }
    Json summary = Json::object();
    summary["source_count"] = static_cast<std::uint64_t>(source_rows.size());
    summary["snapshot_count"] = static_cast<std::uint64_t>(themes.size());
    summary["unique_theme_id_count"] = static_cast<std::uint64_t>(unique_ids.size());
    summary["theme_stock_memberships"] = memberships;
    summary["security_count"] = static_cast<std::uint64_t>(stocks.size());
    summary["count_mismatch_count"] = mismatches;
    Json result = Json::object();
    result["source_options"] = std::move(source_rows);
    result["themes"] = std::move(themes);
    result["summary"] = std::move(summary);
    result["sources"] = std::move(sources);
    result["upstream_health"] = jsn_sources_health(result.at("sources"));
    master_cache_ = {result, std::time(nullptr)};
    return {std::move(result), true, 0};
}

// Per-resource cache for the zttz/ID detail and zttz1/ID chart documents, which
// are fetched only for the selected theme and expire on a shorter TTL.
ThemeLibraryService::FetchResult ThemeLibraryService::fetch_dynamic(
    const std::string& resource, const ThemeLibraryQuery& options) {
    const auto now = std::time(nullptr);
    const auto found = dynamic_cache_.find(resource);
    if (!options.refresh && found != dynamic_cache_.end()) {
        const int age = static_cast<int>(
            std::max<std::time_t>(0, now - found->second.fetched_at));
        if (age < options.detail_cache_ttl_seconds)
            return {found->second.document, false, age};
    }
    auto document = fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
    dynamic_cache_[resource] = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

}  // namespace tdx
