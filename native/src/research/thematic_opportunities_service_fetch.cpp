#include "thematic_opportunities_internal.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <ctime>
#include <set>

namespace tdx {

ThematicOpportunityService::ThematicOpportunityService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

ThematicOpportunityService::FetchResult ThematicOpportunityService::fetch_master(
    const ThematicOpportunityQuery& options) {
    using namespace detail::thematic_opportunities;
    const auto now = std::time(nullptr);
    const int age = master_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - master_cache_.fetched_at)) : 0;
    if (!options.refresh && master_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {master_cache_.document, false, age};

    const auto documents = fetch_jsn_resources_rows(
        resource_paths(), "bi", options.timeout_ms);
    Json groups = Json::array();
    for (const auto& definition : resource_definitions()) {
        if (definition.group_type.empty()) continue;
        const auto& source = document_for_resource(documents, definition.resource);
        const auto normalized = normalize_opportunity_groups(
            source.at("rows"), std::string(definition.group_type), securities_);
        for (const auto& group : normalized.as_array()) groups.push_back(group);
    }
    sort_groups(groups, "name", "asc");

    Json sources = Json::array();
    for (const auto& document : documents.as_array())
        sources.push_back(jsn_source_metadata(document));
    std::set<std::string> securities;
    std::uint64_t memberships = 0, duplicates = 0, industry_count = 0,
        region_count = 0, legacy_count = 0, semantic_mismatches = 0;
    for (const auto& group : groups.as_array()) {
        memberships += static_cast<std::uint64_t>(group.at("member_count").as_number());
        duplicates += static_cast<std::uint64_t>(
            group.at("duplicate_member_count").as_number());
        if (group.at("type").as_string() == "industry") ++industry_count;
        else if (group.at("type").as_string() == "region") ++region_count;
        else ++legacy_count;
        if (group.at("semantic_mismatch").as_bool()) ++semantic_mismatches;
        for (const auto& member : group.at("members").as_array())
            securities.insert(member.at("security_id").as_string());
    }

    Json summary = Json::object();
    summary["group_count"] = static_cast<std::uint64_t>(groups.size());
    summary["industry_group_count"] = industry_count;
    summary["region_group_count"] = region_count;
    summary["legacy_client_theme_count"] = legacy_count;
    summary["semantic_mismatch_count"] = semantic_mismatches;
    summary["group_memberships"] = memberships;
    summary["security_count"] = static_cast<std::uint64_t>(securities.size());
    summary["duplicate_master_memberships"] = duplicates;

    const auto& completed_source = document_for_resource(
        documents, resource_for_role(ResourceRole::completed_hype).resource);
    const auto& active_source = document_for_resource(
        documents, resource_for_role(ResourceRole::active_hype).resource);
    Json document = Json::object();
    document["groups"] = std::move(groups);
    document["completed_hype"] = normalize_completed_hype_rows(
        completed_source.at("rows"), securities_);
    document["active_hype"] = normalize_active_hype_rows(
        active_source.at("rows"), securities_);
    summary["completed_hype_count"] =
        static_cast<std::uint64_t>(document.at("completed_hype").size());
    summary["active_hype_count"] =
        static_cast<std::uint64_t>(document.at("active_hype").size());
    document["summary"] = std::move(summary);
    document["sources"] = std::move(sources);
    document["upstream_health"] = jsn_sources_health(document.at("sources"));
    master_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

ThematicOpportunityService::FetchResult ThematicOpportunityService::fetch_detail(
    const std::string& resource, const ThematicOpportunityQuery& options) {
    const auto now = std::time(nullptr);
    const auto found = detail_cache_.find(resource);
    if (!options.refresh && found != detail_cache_.end()) {
        const int age = static_cast<int>(
            std::max<std::time_t>(0, now - found->second.fetched_at));
        if (age < options.detail_cache_ttl_seconds)
            return {found->second.document, false, age};
    }
    auto document = fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
    detail_cache_[resource] = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

}  // namespace tdx
