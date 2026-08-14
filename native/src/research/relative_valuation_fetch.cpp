#include "relative_valuation_internal.hpp"

#include "tdx/cloud_workflow.hpp"
#include "tdx/pbrpc.hpp"
#include "tdx/tqlex.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

namespace tdx {

RelativeValuationService::RelativeValuationService(
    std::filesystem::path root,
    std::map<std::pair<int, std::string>, Security> securities)
    : root_(std::move(root)), securities_(std::move(securities)) {}

RelativeValuationService::FetchResult RelativeValuationService::fetch_cached(
    std::map<std::string, CachedDocument>& cache, const std::string& key,
    const RelativeValuationQuery& options,
    const std::function<Json()>& loader) {
    using detail::relative_valuation::transient_error;
    FetchResult result;
    const auto now = std::time(nullptr);
    const auto cached = cache.find(key);
    if (cached != cache.end())
        result.age_seconds = static_cast<int>(std::max<std::time_t>(
            0, now - cached->second.fetched_at));
    if (!options.refresh && cached != cache.end() &&
        result.age_seconds < options.cache_ttl_seconds) {
        result.document = cached->second.document;
        result.hit = true;
        return result;
    }
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            result.document = loader();
            cache[key] = CachedDocument{result.document, std::time(nullptr)};
            return result;
        } catch (const Error& error) {
            result.upstream_error = error.what();
            if (!transient_error(result.upstream_error)) throw;
            if (attempt == 2) {
                if (cached == cache.end()) throw;
                result.document = cached->second.document;
                result.hit = true;
                result.stale = true;
                return result;
            }
            std::this_thread::sleep_for(
                std::chrono::milliseconds(250 * (attempt + 1)));
        }
    }
    throw Error("relative valuation retry loop ended unexpectedly");
}

RelativeValuationService::FetchResult RelativeValuationService::fetch_master(
    const RelativeValuationQuery& options,
    const std::map<std::string, std::string>& replacements,
    const std::string& key) {
    return fetch_cached(master_cache_, key, options, [&] {
        const auto upstream = execute_tqlex_config(
            root_, "200000", replacements, {}, {}, "zs_zsgzb.xml", {},
            false, -1, 0, 10,
            cloud_endpoints::tqlex, options.timeout_ms);
        const auto raw_rows = cloud_result_rows(upstream.at("response"));
        Json document = Json::object();
        document["records"] =
            normalize_relative_valuation_master_rows(raw_rows, securities_);
        document["source"] = detail::relative_valuation::source_document(
            upstream, raw_rows, "TQLEX reqformat=2", "200000");
        return document;
    });
}

RelativeValuationService::FetchResult RelativeValuationService::fetch_detail(
    const RelativeValuationQuery& options,
    const std::map<std::string, std::string>& replacements,
    const std::string& key) {
    return fetch_cached(detail_cache_, key, options, [&] {
        const auto upstream = execute_pbrpc_config(
            root_, "200001", replacements, {}, {}, {}, "zs_zsgzb.xml", {},
            cloud_endpoints::tqlex, options.timeout_ms);
        const auto raw_rows = cloud_result_rows(upstream.at("response"));
        Json document = Json::object();
        document["records"] = normalize_relative_valuation_history_rows(raw_rows);
        document["source"] = detail::relative_valuation::source_document(
            upstream, raw_rows, "PBRPC reqformat=22", "200001");
        return document;
    });
}

}  // namespace tdx
