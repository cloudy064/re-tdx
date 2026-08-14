#include "reverse_repo_internal.hpp"

#include "tdx/jsn.hpp"
#include "tdx/market.hpp"

namespace tdx {
ReverseRepoService::ReverseRepoService(std::filesystem::path root,
    std::map<std::pair<int, std::string>, Security> securities) : root_(std::move(root)) {
    names_.securities = std::move(securities);
}
Json ReverseRepoService::fetch_schedule(bool refresh, int ttl, int timeout_ms, bool& fetched) {
    const auto now = std::time(nullptr);
    if (!refresh && schedule_cache_.document.is_object() && now - schedule_cache_.fetched_at < ttl) {
        fetched = false; return schedule_cache_.document;
    }
    auto document = fetch_jsn_resource_rows(detail::reverse_repo::schedule_resource(), "bi", timeout_ms);
    schedule_cache_ = {document, std::time(nullptr)}; fetched = true; return document;
}
Json ReverseRepoService::fetch_quotes(const std::vector<std::string>& securities,
    bool refresh, int ttl, int timeout_ms, bool& fetched) {
    const auto now = std::time(nullptr);
    if (!refresh && quote_cache_.document.is_object() && now - quote_cache_.fetched_at < ttl) {
        fetched = false; return quote_cache_.document;
    }
    auto document = fetch_market_snapshot_document(root_, securities, timeout_ms, &names_);
    quote_cache_ = {document, std::time(nullptr)}; fetched = true; return document;
}
}  // namespace tdx
