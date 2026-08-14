#include "exchange_funds_internal.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/market.hpp"

#include <algorithm>
#include <ctime>
#include <utility>

namespace fs = std::filesystem;

namespace tdx {
namespace fund_detail = exchange_fund_detail;

ExchangeFundService::ExchangeFundService(
    fs::path root,
    std::map<std::pair<int, std::string>, Security> securities,
    fs::path jsn_root)
    : root_(std::move(root)), jsn_root_(std::move(jsn_root)) {
    blocks_.securities = std::move(securities);
}

Json ExchangeFundService::fetch_master(
    const ExchangeFundQuery& options, bool& refreshed, int& age_seconds) {
    const auto now = std::time(nullptr);
    age_seconds = cache_time_
        ? static_cast<int>(std::max<std::time_t>(0, now - cache_time_)) : 0;
    if (!options.refresh && cache_time_ &&
        age_seconds < options.cache_ttl_seconds) {
        refreshed = false;
        return cache_;
    }
    const auto& resources = fund_detail::resource_names();
    Json documents = Json::array();
    if (!options.refresh && !jsn_root_.empty()) {
        try {
            for (const auto& resource : resources)
                documents.push_back(
                    fund_detail::load_local_resource_rows(jsn_root_, resource));
        } catch (...) {
            documents = Json::array();
        }
    }
    if (documents.as_array().empty())
        documents = fetch_jsn_resources_rows(
            resources, "bi", options.timeout_ms);

    Json records = Json::array();
    Json sources = Json::array();
    for (const auto& resource : resources) {
        const auto& document = fund_detail::document_for(documents, resource);
        auto normalized = normalize_exchange_fund_rows(
            resource, document.at("rows"), blocks_.securities);
        sources.push_back(
            fund_detail::source_summary(document, normalized.size()));
        for (auto& row : normalized.as_array())
            records.push_back(std::move(row));
    }
    Json master = Json::object();
    master["records"] = std::move(records);
    master["sources"] = std::move(sources);
    cache_ = master;
    cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return master;
}

Json ExchangeFundService::fetch_quotes(
    const std::vector<std::string>& requested,
    const ExchangeFundQuery& options, bool& refreshed, int& age_seconds) {
    auto sorted = requested;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    std::string key;
    for (const auto& item : sorted) key += item + ',';
    const auto now = std::time(nullptr);
    age_seconds = quote_cache_time_
        ? static_cast<int>(std::max<std::time_t>(0, now - quote_cache_time_))
        : 0;
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
