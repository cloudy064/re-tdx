#include "tdx/curated_data.hpp"
#include "tdx/curated_data_internal.hpp"
#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

#include <ctime>
#include <filesystem>
#include <set>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

using detail::curated_data::resource_count;
using detail::curated_data::resources;
using detail::curated_data::load_local_resource_rows;
using detail::curated_data::document_for;
using detail::curated_data::source_summary;
using detail::curated_data::load_hong_kong_names;

CuratedDataService::CuratedDataService(
    fs::path root,
    std::map<std::pair<int, std::string>, Security> securities,
    fs::path jsn_root)
    : root_(std::move(root)), jsn_root_(std::move(jsn_root)),
      securities_(std::move(securities)),
      hong_kong_names_(load_hong_kong_names(root_)) {}

Json CuratedDataService::fetch_master(
    const CuratedDataQuery& options, bool& refreshed, int& age_seconds) {
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
            for (std::size_t i = 0; i < resource_count; ++i)
                documents.push_back(load_local_resource_rows(jsn_root_,
                                                            resources[i].path));
        } catch (...) {
            documents = Json::array();
        }
    }
    if (documents.as_array().empty()) {
        std::vector<std::string> paths;
        for (std::size_t i = 0; i < resource_count; ++i)
            paths.push_back(resources[i].path);
        documents = fetch_jsn_resources_rows(paths, "bi", options.timeout_ms);
    }

    Json records = Json::array();
    Json sources = Json::array();
    for (std::size_t i = 0; i < resource_count; ++i) {
        const auto& resource_path = resources[i].path;
        const auto& document = document_for(documents, resource_path);
        auto normalized = normalize_curated_data_rows(
            resource_path, document.at("rows"), securities_, hong_kong_names_);
        sources.push_back(source_summary(document, normalized.size()));
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
