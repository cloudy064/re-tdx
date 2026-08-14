// Query-pipeline pieces that need no service state: validation, the catalog
// summary, and the filter/sort/page window.
#include "tdx/economic_indicators_internal.hpp"

#include "tdx/common.hpp"

#include <map>
#include <string>

namespace tdx::economic_indicator_detail {

void validate_indicator_query(EconomicIndicatorQuery& options) {
    options.view = lower_ascii(trim(options.view));
    options.indicator_id = trim(options.indicator_id);
    options.query = trim(options.query);
    options.sort = lower_ascii(trim(options.sort));
    options.order = lower_ascii(trim(options.order));
    if (options.view != "catalog" && options.view != "indicator")
        throw Error("view must be catalog or indicator");
    if (options.view == "indicator" && !valid_indicator_id(options.indicator_id))
        throw Error("indicator view requires a valid M-prefixed indicator_id");
    if (options.offset < 0 || options.offset > 1000000)
        throw Error("offset must be in 0..1000000");
    if (options.limit < 1 || options.limit > 5000)
        throw Error("limit must be in 1..5000");
    if (options.history_limit < 1 || options.history_limit > 10000)
        throw Error("history_limit must be in 1..10000");
    if (options.master_cache_ttl_seconds < 0 || options.master_cache_ttl_seconds > 86400 ||
        options.detail_cache_ttl_seconds < 0 || options.detail_cache_ttl_seconds > 86400 ||
        options.quote_cache_ttl_seconds < 0 || options.quote_cache_ttl_seconds > 3600)
        throw Error("cache TTL is outside the supported range");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
}

Json build_indicator_summary(const Json& indicators) {
    Json summary = Json::object();
    summary["indicator_count"] = static_cast<std::uint64_t>(indicators.size());
    std::map<std::string, std::uint64_t> types, frequencies;
    std::string first_update, last_update;
    for (const auto& row : indicators.as_array()) {
        ++types[row.at("indicator_type").as_string()];
        ++frequencies[row.at("frequency").as_string()];
        const auto& date = row.at("update_date").as_string();
        if (first_update.empty() || (!date.empty() && date < first_update)) first_update = date;
        if (date > last_update) last_update = date;
    }
    Json type_summary = Json::array();
    for (const auto& [name, count] : types) {
        Json row = Json::object(); row["name"] = name; row["count"] = count;
        type_summary.push_back(std::move(row));
    }
    Json frequency_summary = Json::array();
    for (const auto& [name, count] : frequencies) {
        Json row = Json::object(); row["name"] = name; row["count"] = count;
        frequency_summary.push_back(std::move(row));
    }
    summary["by_type"] = std::move(type_summary);
    summary["by_frequency"] = std::move(frequency_summary);
    summary["first_update_date"] = first_update;
    summary["last_update_date"] = last_update;
    return summary;
}

Json page_array(const Json& source, int offset, int limit) {
    Json paged = Json::array();
    for (std::size_t i = static_cast<std::size_t>(offset);
         i < source.size() && paged.size() < static_cast<std::size_t>(limit); ++i)
        paged.push_back(source.as_array()[i]);
    return paged;
}

Json filter_sort_page_catalog(const Json& indicators,
                              const EconomicIndicatorQuery& options,
                              std::uint64_t& matched) {
    Json filtered = Json::array();
    const auto needle = lower_ascii(options.query);
    for (const auto& row : indicators.as_array())
        if (needle.empty() || json_contains(row, needle)) filtered.push_back(row);
    sort_economic_indicator_rows(filtered, options.sort, options.order);
    matched = filtered.size();
    return page_array(filtered, options.offset, options.limit);
}

void record_failure(Json& errors, const Json& resource, const std::string& message) {
    Json failure = Json::object();
    failure["resource"] = resource;
    failure["message"] = message;
    errors.push_back(std::move(failure));
}

}  // namespace tdx::economic_indicator_detail
