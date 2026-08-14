#include "unlocks_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <ctime>
#include <exception>
#include <utility>

namespace fs = std::filesystem;

namespace tdx {

using namespace unlocks_detail;

UnlockService::UnlockService(
    std::map<std::pair<int, std::string>, Security> securities,
    fs::path jsn_root)
    : securities_(std::move(securities)), jsn_root_(std::move(jsn_root)) {}

UnlockService::FetchResult UnlockService::fetch_master(const UnlockQuery& options) {
    const auto now = std::time(nullptr);
    const int age = master_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - master_cache_.fetched_at)) : 0;
    if (!options.refresh && master_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {master_cache_.document, false, age};
    const auto source = fetch_jsn_resource_rows(master_resource, "bi", options.timeout_ms);
    Json document = Json::object();
    document["events"] = normalize_unlock_master_rows(source.at("rows"), securities_);
    document["raw_rows"] = source.at("row_count");
    document["source"] = source_summary(source);
    document["summary"] = summarize_events(
        document.at("events"), static_cast<std::uint64_t>(source.at("row_count").as_number()));
    master_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

UnlockService::FetchResult UnlockService::fetch_recent_large(const UnlockQuery& options) {
    const auto now = std::time(nullptr);
    const int age = recent_large_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(
            0, now - recent_large_cache_.fetched_at)) : 0;
    if (!options.refresh && recent_large_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {recent_large_cache_.document, false, age};
    const auto source = fetch_jsn_resource_rows(
        recent_large_resource, "bi", options.timeout_ms);
    Json document = Json::object();
    document["events"] = normalize_recent_large_unlock_rows(
        source.at("rows"), securities_);
    document["raw_rows"] = source.at("row_count");
    document["source"] = source_summary(source);
    document["summary"] = summarize_events(
        document.at("events"),
        static_cast<std::uint64_t>(source.at("row_count").as_number()));
    recent_large_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

UnlockService::FetchResult UnlockService::fetch_monthly_pressure(
    const UnlockQuery& options) {
    const auto now = std::time(nullptr);
    const int age = monthly_pressure_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(
            0, now - monthly_pressure_cache_.fetched_at)) : 0;
    if (!options.refresh && monthly_pressure_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {monthly_pressure_cache_.document, false, age};
    Json source;
    if (!options.refresh && !jsn_root_.empty()) {
        try { source = load_local_resource(jsn_root_, monthly_pressure_resource); }
        catch (...) {}
    }
    if (!source.is_object())
        source = fetch_jsn_resource_rows(
            monthly_pressure_resource, "bi", options.timeout_ms);
    Json document = Json::object();
    document["months"] = normalize_monthly_unlock_pressure_rows(source.at("rows"));
    document["source"] = source_summary(source);
    monthly_pressure_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

UnlockService::FetchResult UnlockService::fetch_detail(
    const std::string& resource, const UnlockQuery& options) {
    const auto now = std::time(nullptr);
    for (auto item = detail_cache_.begin(); item != detail_cache_.end();) {
        if (now - item->second.fetched_at >= options.detail_cache_ttl_seconds)
            item = detail_cache_.erase(item);
        else ++item;
    }
    for (auto item = detail_failure_cache_.begin(); item != detail_failure_cache_.end();) {
        if (now - item->second.fetched_at >= options.detail_cache_ttl_seconds)
            item = detail_failure_cache_.erase(item);
        else ++item;
    }
    if (options.refresh) {
        detail_cache_.erase(resource);
        detail_failure_cache_.erase(resource);
    }
    const auto cached = detail_cache_.find(resource);
    if (cached != detail_cache_.end()) {
        const int age = static_cast<int>(std::max<std::time_t>(
            0, now - cached->second.fetched_at));
        return {cached->second.document, false, age};
    }
    const auto failure = detail_failure_cache_.find(resource);
    if (failure != detail_failure_cache_.end())
        throw Error(failure->second.message);
    try {
        const auto source = fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
        detail_cache_[resource] = {source, std::time(nullptr)};
        return {source, true, 0};
    } catch (const std::exception& error) {
        detail_failure_cache_[resource] = {error.what(), std::time(nullptr)};
        throw;
    }
}

}  // namespace tdx
