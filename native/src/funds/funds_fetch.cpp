#include "funds_internal.hpp"

#include "tdx/pbrpc.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

namespace tdx {
IntradayFundsService::FetchResult IntradayFundsService::fetch_master(const IntradayFundsQuery& options) {
    using namespace detail::funds; const auto now = std::time(nullptr);
    const int age = master_cache_.fetched_at ? static_cast<int>(std::max<std::time_t>(0, now - master_cache_.fetched_at)) : 0;
    if (!options.refresh && master_cache_.fetched_at && age < options.cache_ttl_seconds) return {master_cache_.document, false, age, false, {}};
    std::string upstream_error;
    for (int attempt = 0; attempt < 3; ++attempt) {
        try { auto document = fetcher_ ? fetcher_("200340", {}, {}, options.timeout_ms)
                : execute_pbrpc_config(root_, "200340", {}, {}, {}, {}, "sszjtj.xml", {}, cloud_endpoints::tqlex, options.timeout_ms);
            (void)response_rows(document); master_cache_ = {document, now}; return {std::move(document), true, 0, false, {}};
        } catch (const Error& error) { upstream_error = error.what(); if (!transient_funds_error(upstream_error)) throw;
            if (attempt != 2) std::this_thread::sleep_for(std::chrono::milliseconds(250 * (attempt + 1))); }
    }
    if (master_cache_.fetched_at) return {master_cache_.document, false, age, true, upstream_error};
    throw Error(upstream_error);
}
IntradayFundsService::FetchResult IntradayFundsService::fetch_detail(const std::string& market,
    const std::string& code, const IntradayFundsQuery& options) {
    using namespace detail::funds; const auto key = market + ":" + code; const auto now = std::time(nullptr);
    auto cached = detail_cache_.find(key); const int age = cached != detail_cache_.end()
        ? static_cast<int>(std::max<std::time_t>(0, now - cached->second.fetched_at)) : 0;
    if (!options.refresh && cached != detail_cache_.end() && cached->second.fetched_at && age < options.cache_ttl_seconds)
        return {cached->second.document, false, age, false, {}};
    std::string upstream_error;
    for (int attempt = 0; attempt < 3; ++attempt) {
        try { auto document = fetcher_ ? fetcher_("200341", market, code, options.timeout_ms)
                : execute_pbrpc_config(root_, "200341", {{"code", "0"}, {"market", "0"}},
                    {{"code_hy", code}, {"market_hy", market}, {"Page", "-1"}}, {}, {}, "sszjtj.xml", {}, cloud_endpoints::tqlex, options.timeout_ms);
            (void)response_rows(document); detail_cache_[key] = {document, now}; return {std::move(document), true, 0, false, {}};
        } catch (const Error& error) { upstream_error = error.what(); if (!transient_funds_error(upstream_error)) throw;
            if (attempt != 2) std::this_thread::sleep_for(std::chrono::milliseconds(250 * (attempt + 1))); }
    }
    if (cached != detail_cache_.end()) return {cached->second.document, false, age, true, upstream_error};
    throw Error(upstream_error);
}
}  // namespace tdx
