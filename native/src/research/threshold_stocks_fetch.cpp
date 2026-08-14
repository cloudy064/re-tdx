#include "threshold_stocks_internal.hpp"

#include "tdx/jsn.hpp"

#include <algorithm>
#include <ctime>

namespace tdx {

ThresholdStocksService::ThresholdStocksService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

ThresholdStocksService::FetchResult ThresholdStocksService::fetch(
    const std::string& resource, const ThresholdStocksQuery& options) {
    const auto now = std::time(nullptr);
    const auto found = cache_.find(resource);
    const int age = found == cache_.end() ? 0 : static_cast<int>(
        std::max<std::time_t>(0, now - found->second.fetched_at));
    if (!options.refresh && found != cache_.end() && age < options.cache_ttl_seconds)
        return {found->second.document, false, age};
    auto document = fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
    cache_[resource] = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

}  // namespace tdx
