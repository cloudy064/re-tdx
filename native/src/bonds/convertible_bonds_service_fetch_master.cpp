#include "convertible_bonds_internal.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <ctime>

namespace fs = std::filesystem;

namespace tdx {

using namespace convertible_bond_detail;

ConvertibleBondService::ConvertibleBondService(
    fs::path root, std::map<std::pair<int, std::string>, Security> securities)
    : root_(std::move(root)), securities_(std::move(securities)) {
    blocks_.securities = securities_;
}

Json ConvertibleBondService::fetch_master(const ConvertibleBondQuery& options,
                                          bool& refreshed, int& age_seconds) {
    const auto now = std::time(nullptr);
    age_seconds = cache_time_
        ? static_cast<int>(std::max<std::time_t>(0, now - cache_time_)) : 0;
    if (!options.refresh && cache_time_ && age_seconds < options.cache_ttl_seconds) {
        refreshed = false;
        return cache_;
    }
    const auto sources = fetch_jsn_resources_rows(master_resources, "bi", options.timeout_ms);
    auto combined_sources = sources;
    Json summaries = Json::array();
    for (const auto& source : sources.as_array())
        summaries.push_back(source_summary(source));
    Json supplement_errors = Json::array();
    Json exchangeable_projection_reconciliation = Json(nullptr);
    Json exchangeable_document = empty_rows_document();
    bool has_exchangeable_document = false;
    try {
        exchangeable_document = fetch_jsn_resource_rows(
            exchangeable_resource, "bi", options.timeout_ms);
        combined_sources.push_back(exchangeable_document);
        summaries.push_back(source_summary(exchangeable_document));
        has_exchangeable_document = true;
    } catch (const std::exception& error) {
        Json failure = Json::object();
        failure["resource"] = exchangeable_resource;
        failure["message"] = error.what();
        supplement_errors.push_back(std::move(failure));
    }
    try {
        const auto projection = fetch_jsn_resource_rows(
            exchangeable_projection_resource, "bi", options.timeout_ms);
        if (!has_exchangeable_document)
            combined_sources.push_back(empty_rows_document());
        combined_sources.push_back(projection);
        summaries.push_back(source_summary(projection));
        exchangeable_projection_reconciliation = raw_document_reconciliation(
            exchangeable_document, projection, exchangeable_resource,
            exchangeable_projection_resource);
    } catch (const std::exception& error) {
        Json failure = Json::object();
        failure["resource"] = exchangeable_projection_resource;
        failure["message"] = error.what();
        supplement_errors.push_back(std::move(failure));
    }
    Json document = Json::object();
    document["rows"] = normalize_convertible_bond_documents(combined_sources, securities_);
    document["sources"] = std::move(summaries);
    document["supplement_errors"] = std::move(supplement_errors);
    document["exchangeable_projection_reconciliation"] =
        std::move(exchangeable_projection_reconciliation);
    cache_ = document;
    cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return document;
}

Json ConvertibleBondService::fetch_pending(const ConvertibleBondQuery& options,
                                           bool& refreshed, int& age_seconds) {
    const auto now = std::time(nullptr);
    age_seconds = pending_cache_time_
        ? static_cast<int>(std::max<std::time_t>(0, now - pending_cache_time_)) : 0;
    if (!options.refresh && pending_cache_time_ &&
        age_seconds < options.cache_ttl_seconds) {
        refreshed = false;
        return pending_cache_;
    }
    const auto source = fetch_jsn_resource_rows(pending_resource, "bi", options.timeout_ms);
    const auto rows = normalize_pending_convertible_bond_document(source, securities_);
    Json sources = Json::array();
    sources.push_back(source_summary(source));
    Json reconciliation = Json::array();
    Json projection_errors = Json::array();
    for (const auto& resource : pending_projection_resources) {
        try {
            const auto projection_source = fetch_jsn_resource_rows(
                resource, "bi", options.timeout_ms);
            const auto projection_rows = normalize_pending_convertible_bond_document(
                projection_source, securities_);
            reconciliation.push_back(security_set_reconciliation(
                rows, projection_rows, "underlying", pending_resource, resource));
            sources.push_back(source_summary(projection_source));
        } catch (const std::exception& error) {
            Json failure = Json::object();
            failure["resource"] = resource;
            failure["message"] = error.what();
            projection_errors.push_back(std::move(failure));
        }
    }
    Json document = Json::object();
    document["rows"] = rows;
    document["sources"] = std::move(sources);
    document["projection_reconciliation"] = std::move(reconciliation);
    document["projection_errors"] = std::move(projection_errors);
    pending_cache_ = document;
    pending_cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return document;
}

}  // namespace tdx

