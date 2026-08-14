#include "state_owned_reform_internal.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/market.hpp"

#include <algorithm>
#include <ctime>
#include <set>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

StateOwnedReformService::StateOwnedReformService(
    fs::path root, std::map<std::pair<int, std::string>, Security> securities)
    : root_(std::move(root)) {
    blocks_.securities = std::move(securities);
}

StateOwnedReformService::FetchResult StateOwnedReformService::fetch_master(
    const StateOwnedReformQuery& options) {
    using namespace state_owned_detail;
    const auto now = std::time(nullptr);
    const int age = master_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - master_cache_.fetched_at)) : 0;
    if (!options.refresh && master_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {master_cache_.document, false, age};

    std::vector<std::string> resources;
    for (const auto& dimension : state_owned_dimensions())
        resources.push_back(dimension.resource);
    resources.push_back(restructuring_resource());
    const auto documents = fetch_jsn_resources_rows(
        resources, "bi", options.timeout_ms);
    Json groups = Json::object();
    Json sources = Json::array();
    std::set<std::pair<int, std::string>> unique;
    std::uint64_t relationships = 0;
    std::uint64_t group_count = 0;
    std::uint64_t mismatch_count = 0;
    for (const auto& dimension : state_owned_dimensions()) {
        const auto& source = document_for_resource(documents, dimension.resource);
        auto normalized = normalize_state_owned_groups(
            source.at("rows"), dimension, blocks_.securities);
        group_count += normalized.size();
        for (const auto& group : normalized.as_array()) {
            if (!group.at("count_matches").as_bool()) ++mismatch_count;
            relationships += static_cast<std::uint64_t>(
                group.at("member_count").as_number());
            for (const auto& member : group.at("members").as_array()) {
                unique.insert({
                    static_cast<int>(member.at("market_id").as_number()),
                    member.at("code").as_string(),
                });
            }
        }
        groups[dimension.name] = std::move(normalized);
        sources.push_back(jsn_source_metadata(source));
    }
    const auto& restructuring_source = document_for_resource(
        documents, restructuring_resource());
    auto restructuring = normalize_state_owned_restructuring(
        restructuring_source.at("rows"), Json::array(), blocks_.securities);
    sources.push_back(jsn_source_metadata(restructuring_source));
    Json summary = Json::object();
    summary["group_count"] = group_count;
    summary["group_relationships"] = relationships;
    summary["unique_grouped_securities"] =
        static_cast<std::uint64_t>(unique.size());
    summary["member_count_mismatches"] = mismatch_count;
    summary["restructuring_rows"] =
        static_cast<std::uint64_t>(restructuring.size());
    Json document = Json::object();
    document["groups"] = std::move(groups);
    document["restructuring"] = std::move(restructuring);
    document["summary"] = std::move(summary);
    document["sources"] = std::move(sources);
    master_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

StateOwnedReformService::FetchResult StateOwnedReformService::fetch_resource(
    const std::string& resource, const StateOwnedReformQuery& options) {
    const auto now = std::time(nullptr);
    const auto found = resource_cache_.find(resource);
    if (!options.refresh && found != resource_cache_.end()) {
        const int age = static_cast<int>(
            std::max<std::time_t>(0, now - found->second.fetched_at));
        if (age < options.detail_cache_ttl_seconds)
            return {found->second.document, false, age};
    }
    auto document = fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
    resource_cache_[resource] = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

StateOwnedReformService::FetchResult StateOwnedReformService::fetch_quotes(
    const std::vector<std::string>& securities,
    const StateOwnedReformQuery& options) {
    auto requested = securities;
    std::sort(requested.begin(), requested.end());
    requested.erase(std::unique(requested.begin(), requested.end()), requested.end());
    std::string key;
    for (const auto& security : requested) key += security + ',';
    const auto now = std::time(nullptr);
    const auto found = quote_cache_.find(key);
    if (!options.refresh && found != quote_cache_.end()) {
        const int age = static_cast<int>(
            std::max<std::time_t>(0, now - found->second.fetched_at));
        if (age < options.quote_cache_ttl_seconds)
            return {found->second.document, false, age};
    }
    auto document = fetch_market_snapshot_document(
        root_, requested, options.timeout_ms, &blocks_);
    quote_cache_[key] = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

}  // namespace tdx
