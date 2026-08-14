#include "forecasts_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <ctime>
#include <set>
#include <vector>

namespace tdx {

ForecastService::ForecastService(BlockData data)
    : securities_(std::move(data.securities)) {
    std::set<std::string> first_level;
    for (const auto& block : data.blocks) {
        if (block.family != "research-industry" || block.level != 1 ||
            !forecast_detail::industry_code(block.block_code))
            continue;
        first_level.insert(block.block_code);
        industry_names_[block.block_code] = block.name;
    }
    for (const auto& member : data.members) {
        if (member.family != "research-industry" ||
            !first_level.count(member.block_code))
            continue;
        security_industries_.emplace(
            std::make_pair(member.market_id, member.code), member.block_code);
    }
}

ForecastService::FetchResult ForecastService::fetch_core(
    const ForecastQuery& options) {
    using namespace forecast_detail;
    const auto now = std::time(nullptr);
    const int age = core_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - core_cache_.fetched_at)) : 0;
    if (!options.refresh && core_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {core_cache_.document, false, age};

    std::vector<std::string> resources;
    resources.reserve(resource_definitions().size());
    for (const auto& resource : resource_definitions())
        resources.emplace_back(resource.path);
    const auto documents = fetch_jsn_resources_rows(
        resources, "bi", options.timeout_ms);
    auto rows = [&](ResourceKind kind) -> const Json& {
        return document_for_resource(documents, resource_name(kind)).at("rows");
    };

    Json document = Json::object();
    document["industries"] = normalize_forecast_industry_rows(
        rows(ResourceKind::industry), industry_names_);
    document["hong_kong"] = normalize_hong_kong_forecast_rows(
        rows(ResourceKind::hong_kong));
    document["latest"] = normalize_forecast_security_rows(
        rows(ResourceKind::latest), securities_);
    document["summary"] = build_core_summary(document);
    Json sources = Json::array();
    for (const auto& resource : resource_definitions()) {
        sources.push_back(source_summary(
            document_for_resource(documents, resource.path)));
    }
    document["sources"] = std::move(sources);
    core_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

ForecastService::FetchResult ForecastService::fetch_resource(
    const std::string& resource, const ForecastQuery& options) {
    const auto now = std::time(nullptr);
    for (auto item = resource_cache_.begin(); item != resource_cache_.end();) {
        if (now - item->second.fetched_at >= options.detail_cache_ttl_seconds)
            item = resource_cache_.erase(item);
        else ++item;
    }
    for (auto item = failure_cache_.begin(); item != failure_cache_.end();) {
        if (now - item->second.second >= options.detail_cache_ttl_seconds)
            item = failure_cache_.erase(item);
        else ++item;
    }
    if (options.refresh) {
        resource_cache_.erase(resource);
        failure_cache_.erase(resource);
    }
    const auto cached = resource_cache_.find(resource);
    if (cached != resource_cache_.end()) {
        const int age = static_cast<int>(std::max<std::time_t>(
            0, now - cached->second.fetched_at));
        return {cached->second.document, false, age};
    }
    const auto failed = failure_cache_.find(resource);
    if (failed != failure_cache_.end()) throw Error(failed->second.first);
    try {
        const auto document = fetch_jsn_resource_rows(
            resource, "bi", options.timeout_ms);
        resource_cache_[resource] = {document, std::time(nullptr)};
        return {document, true, 0};
    } catch (const std::exception& error) {
        failure_cache_[resource] = {error.what(), std::time(nullptr)};
        throw;
    }
}

}  // namespace tdx
