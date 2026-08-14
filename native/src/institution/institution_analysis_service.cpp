#include "institution_analysis_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

#include <ctime>
#include <utility>
#include <vector>

namespace tdx {

using namespace institution_analysis_detail;

InstitutionAnalysisService::InstitutionAnalysisService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

Json InstitutionAnalysisService::query(const InstitutionAnalysisQuery& options) {
    const auto view_id = lower_ascii(trim(options.view));
    if (view_id == "catalog") return catalog_document();
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400)
        throw Error("cache_ttl_seconds must be in 0..86400");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");

    std::vector<const ViewSpec*> selected;
    if (view_id == "security") {
        for (const auto& view : views()) selected.push_back(&view);
    } else {
        const auto* view = find_view(view_id);
        if (!view) throw Error("unknown institution-analysis view: " + options.view);
        selected.push_back(view);
    }
    const auto now = std::time(nullptr);
    std::vector<std::string> stale;
    for (const auto* view : selected) {
        const auto found = cache_.find(view->resource);
        if (options.refresh || found == cache_.end() ||
            now - found->second.fetched_at >= options.cache_ttl_seconds)
            stale.push_back(view->resource);
    }
    if (!stale.empty()) {
        const auto fetched = fetch_jsn_resources_rows(stale, "bi", options.timeout_ms);
        if (!fetched.is_array() || fetched.as_array().size() != stale.size())
            throw Error("institution-analysis resource batch is incomplete");
        for (std::size_t index = 0; index < stale.size(); ++index)
            cache_[stale[index]] = {fetched.as_array()[index], std::time(nullptr)};
    }
    Json documents = Json::array();
    for (const auto* view : selected) {
        const auto found = cache_.find(view->resource);
        if (found == cache_.end()) throw Error("institution-analysis cache is incomplete");
        documents.push_back(found->second.document);
    }
    auto result = compose_institution_analysis_document(options, documents, securities_);
    Json cache = Json::object();
    cache["refreshed"] = !stale.empty();
    cache["ttl_seconds"] = options.cache_ttl_seconds;
    cache["resource_count"] = static_cast<std::uint64_t>(selected.size());
    result["cache"] = std::move(cache);
    return result;
}

}  // namespace tdx
