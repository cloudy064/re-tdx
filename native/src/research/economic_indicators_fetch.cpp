// Constructor and the two cache domains: JSN resources keyed by resource path,
// and public L1 quote snapshots keyed by the sorted security list.
#include "tdx/economic_indicators_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/market.hpp"

#include <algorithm>
#include <ctime>
#include <string>

namespace fs = std::filesystem;

namespace tdx {

EconomicIndicatorService::EconomicIndicatorService(
    fs::path root, std::map<std::pair<int, std::string>, Security> securities)
    : root_(std::move(root)) {
    blocks_.securities = std::move(securities);
}

EconomicIndicatorService::FetchResult EconomicIndicatorService::fetch_resource(
    const std::string& resource, bool refresh, int ttl_seconds, int timeout_ms) {
    const auto now = std::time(nullptr);
    const auto found = resource_cache_.find(resource);
    if (!refresh && found != resource_cache_.end()) {
        const int age = static_cast<int>(
            std::max<std::time_t>(0, now - found->second.fetched_at));
        if (age < ttl_seconds) return {found->second.document, false, age};
    }
    auto document = fetch_jsn_resource_rows(resource, "bi", timeout_ms);
    resource_cache_[resource] = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

EconomicIndicatorService::FetchResult EconomicIndicatorService::fetch_quotes(
    const std::vector<std::string>& securities, const EconomicIndicatorQuery& options) {
    // The cache key is the deduplicated, sorted request list, so the same set of
    // securities hits the same entry regardless of the order they arrived in.
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
