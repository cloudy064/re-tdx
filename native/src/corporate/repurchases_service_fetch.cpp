#include "repurchases_internal.hpp"

#include "tdx/jsn.hpp"

#include <algorithm>

namespace tdx {

using namespace detail::repurchases;

RepurchaseService::RepurchaseService(
    std::map<std::pair<int, std::string>, Security> securities,
    std::filesystem::path jsn_root)
    : securities_(std::move(securities)), jsn_root_(std::move(jsn_root)) {}

RepurchaseService::FetchResult RepurchaseService::fetch_core(
    const RepurchaseQuery& options) {
    const auto now = std::time(nullptr);
    const int age = core_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - core_cache_.fetched_at)) : 0;
    if (!options.refresh && core_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {core_cache_.document, false, age};

    std::vector<std::string> resources;
    for (const auto& spec : core_resources()) resources.push_back(spec.resource);
    for (const auto& spec : annual_specs()) resources.push_back(spec.resource);
    Json documents = Json::array();
    if (!jsn_root_.empty()) {
        try {
            for (const auto& resource : resources)
                documents.push_back(load_local_resource_rows(jsn_root_, resource));
        } catch (...) {
            documents = Json::array();
        }
    }
    if (!documents.size())
        documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);
    Json document = Json::object(), sources = Json::array();
    for (const auto& spec : core_resources()) {
        const auto& source = document_for_resource(documents, spec.resource);
        switch (spec.role) {
            case CoreResourceRole::plans:
                document[spec.document_key] = normalize_repurchase_plan_rows(
                    source.at("rows"), securities_);
                break;
            case CoreResourceRole::monthly:
                document[spec.document_key] = normalize_repurchase_month_rows(
                    source.at("rows"));
                break;
            case CoreResourceRole::hong_kong:
                document[spec.document_key] = normalize_hk_repurchase_rows(
                    source.at("rows"), securities_);
                break;
        }
        sources.push_back(source_summary(source));
    }
    for (const auto& spec : annual_specs()) {
        const auto& source = document_for_resource(documents, spec.resource);
        document[std::string("annual_") + spec.id] =
            normalize_repurchase_annual_rows(source.at("rows"), spec.id);
        sources.push_back(source_summary(source));
    }
    Json summary = summarize_plans(document.at("plans"));
    summary["monthly_points"] = static_cast<std::uint64_t>(
        document.at("monthly").size());
    summary["hong_kong_securities"] = static_cast<std::uint64_t>(
        document.at("hong_kong").size());
    summary["annual_years"] = static_cast<std::uint64_t>(
        document.at("annual_a").size());
    document["summary"] = std::move(summary);
    document["sources"] = std::move(sources);
    core_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

RepurchaseService::FetchResult RepurchaseService::fetch_resource(
    const std::string& resource, const RepurchaseQuery& options) {
    const auto now = std::time(nullptr);
    for (auto item = resource_cache_.begin(); item != resource_cache_.end();) {
        if (now - item->second.fetched_at >= options.detail_cache_ttl_seconds)
            item = resource_cache_.erase(item);
        else
            ++item;
    }
    for (auto item = failure_cache_.begin(); item != failure_cache_.end();) {
        if (now - item->second.fetched_at >= options.detail_cache_ttl_seconds)
            item = failure_cache_.erase(item);
        else
            ++item;
    }
    if (options.refresh) {
        resource_cache_.erase(resource);
        failure_cache_.erase(resource);
    }
    const auto cached = resource_cache_.find(resource);
    if (cached != resource_cache_.end()) {
        const int cached_age = static_cast<int>(std::max<std::time_t>(
            0, now - cached->second.fetched_at));
        return {cached->second.document, false, cached_age};
    }
    const auto failure = failure_cache_.find(resource);
    if (failure != failure_cache_.end()) throw Error(failure->second.message);
    try {
        Json source;
        if (!jsn_root_.empty() &&
            std::filesystem::is_regular_file(jsn_root_ / native_path(resource)))
            source = load_local_resource_rows(jsn_root_, resource);
        else
            source = fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
        resource_cache_[resource] = {source, std::time(nullptr)};
        return {source, true, 0};
    } catch (const std::exception& error) {
        failure_cache_[resource] = {error.what(), std::time(nullptr)};
        throw;
    }
}

}  // namespace tdx
