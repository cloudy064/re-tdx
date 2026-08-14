#include "anomaly_risk_internal.hpp"

#include "tdx/tqlex.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

namespace tdx {

AnomalyRiskService::AnomalyRiskService(std::filesystem::path root, BlockData blocks)
    : root_(std::move(root)), blocks_(std::move(blocks)) {}

Json AnomalyRiskService::query(const AnomalyRiskQuery& input) {
    using namespace detail::anomaly_risk;
    const auto plan = make_query_plan(input);
    const auto now = std::time(nullptr);
    const auto cached = cache_.find(plan.cache_key);
    if (!plan.options.refresh && cached != cache_.end()) {
        const int age = static_cast<int>(std::max<std::time_t>(
            0, now - cached->second.fetched_at));
        if (age < plan.options.cache_ttl_seconds) {
            auto result = cached->second.document;
            result["cache"]["hit"] = true;
            result["cache"]["age_seconds"] = age;
            return result;
        }
    }

    Json upstream;
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            upstream = execute_tqlex_config(
                root_, plan.view->request_id, {}, plan.overrides, {},
                plan.view->source_file, {}, plan.view->all_pages,
                plan.view->page, plan.view->page_size, plan.view->max_pages,
                cloud_endpoints::tqlex,
                plan.options.timeout_ms);
            break;
        } catch (const Error& error) {
            const std::string message = error.what();
            if (!transient_error(message)) throw;
            if (attempt == 2) {
                if (cached != cache_.end()) {
                    auto stale = cached->second.document;
                    stale["availability"] = "stale-cache";
                    stale["cache"]["hit"] = true;
                    stale["cache"]["stale"] = true;
                    stale["cache"]["age_seconds"] = static_cast<std::uint64_t>(
                        std::max<std::time_t>(0, now - cached->second.fetched_at));
                    stale["cache"]["upstream_error"] = message;
                    return stale;
                }
                throw;
            }
            std::this_thread::sleep_for(
                std::chrono::milliseconds(250 * (attempt + 1)));
        }
    }
    auto result = build_live_document(upstream, plan, blocks_);
    cache_[plan.cache_key] = CachedDocument{result, std::time(nullptr)};
    return result;
}

}  // namespace tdx
