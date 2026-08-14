#include "special_situations_internal.hpp"

namespace tdx {
namespace fs = std::filesystem;
using namespace special_situations_detail;

SpecialSituationService::SpecialSituationService(
    fs::path root,
    std::map<std::pair<int, std::string>, Security> securities,
    fs::path jsn_root)
    : root_(std::move(root)), jsn_root_(std::move(jsn_root)) {
    blocks_.securities = std::move(securities);
}

Json SpecialSituationService::fetch_master(
    const SpecialSituationQuery& options, bool& refreshed, int& age_seconds) {
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
            for (const auto& resource : resource_names())
                documents.push_back(load_local_resource_rows(jsn_root_, resource));
        } catch (...) {
            documents = Json::array();
        }
    }
    if (documents.as_array().empty())
        documents = fetch_jsn_resources_rows(resource_names(), "bi", options.timeout_ms);

    Json rows = Json::array();
    Json sources = Json::array();
    for (const auto& resource : resource_names()) {
        const auto& document = document_for(documents, resource);
        auto normalized = normalize_special_situation_rows(
            resource, document.at("rows"), blocks_.securities);
        for (auto& row : normalized.as_array()) rows.push_back(std::move(row));
        sources.push_back(source_summary(document));
    }
    Json master = Json::object();
    master["rows"] = std::move(rows);
    master["sources"] = std::move(sources);
    cache_ = master;
    cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return master;
}

Json SpecialSituationService::fetch_quotes(
    const std::vector<std::string>& requested,
    const SpecialSituationQuery& options, bool& refreshed, int& age_seconds) {
    auto sorted = requested;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    std::string key;
    for (const auto& item : sorted) key += item + ',';
    const auto now = std::time(nullptr);
    age_seconds = quote_cache_time_
        ? static_cast<int>(std::max<std::time_t>(0, now - quote_cache_time_)) : 0;
    if (!options.refresh && quote_cache_time_ && key == quote_cache_key_ &&
        age_seconds < options.quote_cache_ttl_seconds) {
        refreshed = false;
        return quote_cache_;
    }
    quote_cache_ = fetch_market_snapshot_document(
        root_, sorted, options.timeout_ms, &blocks_);
    quote_cache_key_ = std::move(key);
    quote_cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return quote_cache_;
}

}  // namespace tdx
