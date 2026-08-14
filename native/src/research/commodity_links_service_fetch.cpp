#include "commodity_links_internal.hpp"

namespace tdx {

CommodityLinksService::CommodityLinksService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

Json CommodityLinksService::fetch(const std::string& resource, bool refresh,
                                  int cache_ttl_seconds, int timeout_ms,
                                  bool& fetched) {
    const auto now = std::time(nullptr);
    const auto found = cache_.find(resource);
    if (!refresh && found != cache_.end() &&
        now - found->second.fetched_at < cache_ttl_seconds) {
        fetched = false;
        return found->second.document;
    }
    auto document = fetch_jsn_resource_rows(resource, "bi", timeout_ms);
    cache_[resource] = {document, std::time(nullptr)};
    fetched = true;
    return document;
}

}  // namespace tdx
