#include "limit_review_internal.hpp"

#include "tdx/jsn.hpp"

#include <algorithm>
#include <ctime>

namespace tdx {

LimitReviewService::LimitReviewService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

Json LimitReviewService::fetch_resource(const std::string& resource,
                                        const LimitReviewQuery& options,
                                        bool allow_missing) {
    using namespace detail::limit_review;
    const auto now = std::time(nullptr);
    const auto cached = cache_.find(resource);
    const int age = cached == cache_.end() ? 0 :
        static_cast<int>(std::max<std::time_t>(0, now - cached->second.fetched_at));
    if (!options.refresh && cached != cache_.end() &&
        age < options.cache_ttl_seconds)
        return cached->second.document;
    try {
        auto document = fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
        cache_[resource] = {document, std::time(nullptr)};
        return document;
    } catch (const std::exception& error) {
        if (!allow_missing || !missing_resource_error(error.what())) throw;
        auto document = missing_document(resource, error.what());
        cache_[resource] = {document, std::time(nullptr)};
        return document;
    }
}

}  // namespace tdx
