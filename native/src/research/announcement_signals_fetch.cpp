#include "tdx/announcement_signals.hpp"
#include "tdx/announcement_signals_internal.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <ctime>

namespace tdx {

using detail::announcement_signals::history_resource;

AnnouncementSignalsService::AnnouncementSignalsService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

Json AnnouncementSignalsService::fetch(const std::string& resource, bool refresh,
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
