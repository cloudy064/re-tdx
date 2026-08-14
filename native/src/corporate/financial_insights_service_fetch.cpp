#include "financial_insights_internal.hpp"

#include "tdx/jsn.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

namespace tdx {

FinancialInsightsService::FinancialInsightsService(
    fs::path root, std::map<std::pair<int, std::string>, Security> securities,
    fs::path jsn_root)
    : root_(std::move(root)), jsn_root_(std::move(jsn_root)),
      securities_(std::move(securities)) {}

Json FinancialInsightsService::fetch_master(
    const FinancialInsightsQuery& options, bool& refreshed, int& age_seconds) {
    const auto now = std::time(nullptr);
    age_seconds = cache_time_
        ? static_cast<int>(std::max<std::time_t>(0, now - cache_time_)) : 0;
    if (!options.refresh && cache_time_ && age_seconds < options.cache_ttl_seconds) {
        refreshed = false;
        return cache_;
    }
    Json documents = Json::array();
    if (!options.refresh && !jsn_root_.empty()) {
        try {
            for (const auto& resource : detail::financial_insights::resource_paths())
                documents.push_back(detail::financial_insights::load_local_resource_rows(jsn_root_, resource));
        } catch (...) { documents = Json::array(); }
    }
    if (documents.as_array().empty())
        documents = fetch_jsn_resources_rows(detail::financial_insights::resource_paths(), "bi", options.timeout_ms);
    Json records = Json::array(), sources = Json::array();
    for (const auto& resource : detail::financial_insights::resource_paths()) {
        const auto& document = detail::financial_insights::document_for(documents, resource);
        auto normalized = normalize_financial_insight_rows(
            resource, document.at("rows"), securities_);
        sources.push_back(detail::financial_insights::source_summary(document, normalized.size()));
        for (auto& row : normalized.as_array()) records.push_back(std::move(row));
    }
    Json result = Json::object();
    result["records"] = std::move(records);
    result["sources"] = std::move(sources);
    cache_ = result;
    cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return result;
}

}  // namespace tdx
