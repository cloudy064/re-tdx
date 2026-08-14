#include "ratings_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

#include <algorithm>
#include <ctime>
#include <exception>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

using namespace ratings_detail;

RatingService::RatingService(fs::path root, BlockData data)
    : securities_(std::move(data.securities)),
      hong_kong_names_(load_hong_kong_security_names(root)) {
    for (const auto& block : data.blocks) {
        if (block.family == "research-industry" && block.level == 1 &&
            !block.block_code.empty())
            industry_names_[block.block_code] = block.name;
    }
    for (const auto& member : data.members) {
        if (member.family != "research-industry") continue;
        const auto found = industry_names_.find(member.block_code);
        if (found != industry_names_.end())
            security_industries_.emplace(
                std::make_pair(member.market_id, member.code), member.block_code);
    }
}

RatingService::FetchResult RatingService::fetch_master(
    const RatingQuery& options) {
    const auto now = std::time(nullptr);
    const int age = master_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - master_cache_.fetched_at)) : 0;
    if (!options.refresh && master_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {master_cache_.document, false, age};
    const std::vector<std::string> resources{
        std::string(hong_kong_master_resource),
        std::string(united_states_master_resource),
        std::string(industry_master_resource)};
    const auto documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);
    const auto& hong_kong_source = document_for_resource(
        documents, hong_kong_master_resource);
    const auto& united_states_source = document_for_resource(
        documents, united_states_master_resource);
    const auto& industry_source = document_for_resource(
        documents, industry_master_resource);
    Json document = Json::object();
    document["hong_kong"] = normalize_hong_kong_rating_rows(
        hong_kong_source.at("rows"), hong_kong_names_);
    document["united_states"] = normalize_united_states_rating_rows(
        united_states_source.at("rows"));
    document["industries"] = normalize_industry_rating_rows(
        industry_source.at("rows"), industry_names_);
    document["hong_kong_summary"] = hong_kong_summary(document.at("hong_kong"));
    document["united_states_summary"] = united_states_summary(
        document.at("united_states"));
    document["industry_summary"] = industry_summary(document.at("industries"));
    Json sources = Json::array();
    sources.push_back(source_summary(hong_kong_source));
    sources.push_back(source_summary(united_states_source));
    sources.push_back(source_summary(industry_source));
    document["sources"] = std::move(sources);
    master_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

RatingService::FetchResult RatingService::fetch_resource(
    const std::string& resource, const RatingQuery& options) {
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
        const auto document = fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
        resource_cache_[resource] = {document, std::time(nullptr)};
        return {document, true, 0};
    } catch (const std::exception& error) {
        failure_cache_[resource] = {error.what(), std::time(nullptr)};
        throw;
    }
}

}  // namespace tdx
