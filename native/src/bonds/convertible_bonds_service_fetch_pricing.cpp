#include "convertible_bonds_internal.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/market.hpp"

#include <algorithm>
#include <ctime>

namespace tdx {

using namespace convertible_bond_detail;

Json ConvertibleBondService::fetch_subscriptions(
    const ConvertibleBondQuery& options, bool& refreshed, int& age_seconds) {
    const auto now = std::time(nullptr);
    age_seconds = subscription_cache_time_
        ? static_cast<int>(std::max<std::time_t>(0, now - subscription_cache_time_)) : 0;
    if (!options.refresh && subscription_cache_time_ &&
        age_seconds < options.cache_ttl_seconds) {
        refreshed = false;
        return subscription_cache_;
    }
    const auto source = fetch_jsn_resource_rows(
        subscription_resource, "bi", options.timeout_ms);
    const auto rows = normalize_convertible_bond_subscription_document(
        source, securities_);
    Json sources = Json::array();
    sources.push_back(source_summary(source));
    Json projection_errors = Json::array();
    Json projection = Json::array();
    Json reconciliation = Json::object();
    try {
        const auto projection_source = fetch_jsn_resource_rows(
            new_bond_projection_resource, "bi", options.timeout_ms);
        const auto projection_rows = normalize_new_convertible_bond_projection_document(
            projection_source, securities_);
        const auto reconciled = reconcile_new_convertible_bond_projection(
            rows, projection_rows);
        projection = reconciled.at("rows");
        reconciliation = reconciled.at("summary");
        sources.push_back(source_summary(projection_source));
    } catch (const std::exception& error) {
        Json failure = Json::object();
        failure["resource"] = new_bond_projection_resource;
        failure["message"] = error.what();
        projection_errors.push_back(std::move(failure));
    }
    Json document = Json::object();
    document["rows"] = rows;
    document["new_bond_projection"] = std::move(projection);
    document["new_bond_reconciliation"] = std::move(reconciliation);
    document["sources"] = std::move(sources);
    document["projection_errors"] = std::move(projection_errors);
    subscription_cache_ = document;
    subscription_cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return document;
}

Json ConvertibleBondService::fetch_pricing(const ConvertibleBondQuery& options,
                                           bool& refreshed, int& age_seconds) {
    const auto now = std::time(nullptr);
    age_seconds = pricing_cache_time_
        ? static_cast<int>(std::max<std::time_t>(0, now - pricing_cache_time_)) : 0;
    if (!options.refresh && pricing_cache_time_ &&
        age_seconds < options.cache_ttl_seconds) {
        refreshed = false;
        return pricing_cache_;
    }
    const auto source = fetch_jsn_resource_rows(pricing_resource, "bi", options.timeout_ms);
    Json document = Json::object();
    document["rows"] = source.at("rows");
    document["source"] = source_summary(source);
    pricing_cache_ = document;
    pricing_cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return document;
}

Json ConvertibleBondService::fetch_pricing_quotes(
    const std::vector<std::string>& requested,
    const ConvertibleBondQuery& options, bool& refreshed, int& age_seconds) {
    std::string cache_key;
    for (const auto& security : requested) cache_key += security + '|';
    const auto now = std::time(nullptr);
    const auto cached = pricing_quote_cache_.find(cache_key);
    age_seconds = cached != pricing_quote_cache_.end()
        ? static_cast<int>(std::max<std::time_t>(
            0, now - cached->second.fetched_at)) : 0;
    if (!options.refresh && cached != pricing_quote_cache_.end() &&
        cached->second.document.is_object() &&
        age_seconds < options.quote_cache_ttl_seconds) {
        refreshed = false;
        return cached->second.document;
    }
    auto document = fetch_market_snapshot_document(
        root_, requested, options.timeout_ms, &blocks_);
    if (pricing_quote_cache_.size() >= 32) pricing_quote_cache_.erase(pricing_quote_cache_.begin());
    pricing_quote_cache_[cache_key] = {document, std::time(nullptr)};
    refreshed = true;
    age_seconds = 0;
    return document;
}

}  // namespace tdx

