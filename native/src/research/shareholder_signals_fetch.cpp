#include "shareholder_signals_internal.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>

namespace tdx {

ShareholderSignalsService::ShareholderSignalsService(
    std::filesystem::path root,
    std::map<std::pair<int, std::string>, Security> securities,
    std::filesystem::path jsn_root)
    : root_(std::move(root)), jsn_root_(std::move(jsn_root)),
      securities_(std::move(securities)) {}

Json ShareholderSignalsService::fetch_master(
    const ShareholderSignalsQuery& options, bool& refreshed, int& age_seconds) {
    using namespace detail::shareholder_signals;
    const auto now = std::time(nullptr);
    age_seconds = cache_time_
        ? static_cast<int>(std::max<std::time_t>(0, now - cache_time_)) : 0;
    if (!options.refresh && cache_time_ && age_seconds < options.cache_ttl_seconds) {
        refreshed = false;
        return cache_;
    }
    std::vector<std::string> resources;
    for (const auto& spec : all_resources()) resources.push_back(spec.resource);
    Json documents = Json::array();
    if (!options.refresh && !jsn_root_.empty()) {
        try {
            for (const auto& resource : resources)
                documents.push_back(load_local_resource_rows(jsn_root_, resource));
        } catch (...) { documents = Json::array(); }
    }
    if (documents.as_array().empty())
        documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);

    Json records = Json::array();
    Json directory = Json::array();
    Json sources = Json::array();
    for (const auto& spec : all_resources()) {
        const auto& document = document_for(documents, spec.resource);
        if (spec.kind == ResourceKind::investor_directory) {
            directory = normalize_notable_investor_directory_rows(document.at("rows"));
            sources.push_back(source_summary(document, directory.size()));
            continue;
        }
        auto normalized = normalize_shareholder_signal_rows(
            spec.resource, document.at("rows"), securities_);
        sources.push_back(source_summary(document, normalized.size()));
        for (auto& row : normalized.as_array()) records.push_back(std::move(row));
    }
    Json result = Json::object();
    result["records"] = std::move(records);
    result["investor_directory"] = std::move(directory);
    result["sources"] = std::move(sources);
    cache_ = result;
    cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return result;
}

}  // namespace tdx
