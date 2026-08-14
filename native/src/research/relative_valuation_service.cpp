#include "relative_valuation_internal.hpp"

namespace tdx {

Json RelativeValuationService::query(const RelativeValuationQuery& input) {
    using namespace detail::relative_valuation;
    const auto plan = make_query_plan(input);
    const auto master = fetch_master(
        plan.options, plan.replacements, plan.master_cache_key);

    const Json* selected = nullptr;
    std::size_t matches = 0;
    for (const auto& record : master.document.at("records").as_array()) {
        if (!plan.options.code.empty() &&
            record.at("security").at("code").as_string() == plan.options.code) {
            selected = &record;
            ++matches;
        }
    }
    if (!plan.options.code.empty() && !selected)
        throw Error("selected index is absent from the current relative-valuation master table");
    if (matches > 1)
        throw Error("selected index code is ambiguous in the current master table");

    Json history = Json::array();
    Json detail_source = Json(nullptr);
    FetchResult detail;
    std::size_t full_history_count = 0;
    if (selected) {
        auto replacements = plan.replacements;
        replacements["Code"] = plan.options.code;
        detail = fetch_detail(
            plan.options, replacements,
            plan.master_cache_key + "|" + plan.options.code);
        history = detail.document.at("records");
        detail_source = detail.document.at("source");
        full_history_count = history.size();
        if (history.size() > static_cast<std::size_t>(plan.options.limit))
            history.as_array().erase(
                history.as_array().begin(),
                history.as_array().end() -
                    static_cast<std::ptrdiff_t>(plan.options.limit));
    }

    Json counts = Json::object();
    counts["indices"] =
        static_cast<std::uint64_t>(master.document.at("records").size());
    counts["full_history_points"] =
        static_cast<std::uint64_t>(full_history_count);
    counts["returned_history_points"] =
        static_cast<std::uint64_t>(history.size());
    counts["history_truncated"] = full_history_count > history.size();

    auto cache_entry = [&](const FetchResult& fetch) {
        Json value = Json::object();
        value["hit"] = fetch.hit;
        value["stale"] = fetch.stale;
        value["age_seconds"] = fetch.age_seconds;
        if (!fetch.upstream_error.empty())
            value["upstream_error"] = fetch.upstream_error;
        return value;
    };
    Json cache = Json::object();
    cache["ttl_seconds"] = plan.options.cache_ttl_seconds;
    cache["master"] = cache_entry(master);
    cache["detail"] = selected ? cache_entry(detail) : Json(nullptr);

    Json sources = Json::array();
    sources.push_back(master.document.at("source"));
    if (selected) sources.push_back(detail_source);

    Json result = Json::object();
    result["schema"] = "tdx-relative-valuation-native-v1";
    result["availability"] = master.stale || detail.stale
        ? "stale-cache" : "live";
    result["generated_at"] = now_text();
    result["mode"] = selected ? "index-history" : "master";
    result["parameters"] = parameters_document(plan);
    result["methodology"] = methodology_document();
    result["indices"] = master.document.at("records");
    result["selected"] = selected ? *selected : Json(nullptr);
    result["history"] = history;
    result["summary"] = selected ? range_summary(history) : Json::object();
    result["counts"] = std::move(counts);
    result["sources"] = std::move(sources);
    result["cache"] = std::move(cache);
    return result;
}

}  // namespace tdx
