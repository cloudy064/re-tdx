#include "ownership_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

#include <algorithm>
#include <ctime>
#include <utility>
#include <vector>

namespace tdx {

using namespace ownership_detail;
OwnershipService::OwnershipService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

OwnershipService::FetchResult OwnershipService::fetch_core(
    const OwnershipQuery& options) {
    const auto now = std::time(nullptr);
    const int age = core_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - core_cache_.fetched_at)) : 0;
    if (!options.refresh && core_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {core_cache_.document, false, age};

    const std::vector<std::string> resources{
        change_increase_resource, change_decrease_resource,
        plan_increase_resource, plan_decrease_resource,
        commitment_resource, insider_resource,
        change_month_resource, change_year_resource,
        pledge_latest_resource, pledge_warning_resource,
        pledge_liquidation_resource, pledge_release_resource,
        pledge_month_resource, pledge_trust_resource, pledge_broker_resource};
    const auto documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);
    auto rows = [&](const char* resource) -> const Json& {
        return document_for_resource(documents, resource).at("rows");
    };

    Json document = Json::object();
    document["change_increase"] = normalize_ownership_change_rows(
        rows(change_increase_resource), "increase", securities_);
    document["change_decrease"] = normalize_ownership_change_rows(
        rows(change_decrease_resource), "decrease", securities_);
    document["plan_increase"] = normalize_ownership_plan_rows(
        rows(plan_increase_resource), "increase", securities_);
    document["plan_decrease"] = normalize_ownership_plan_rows(
        rows(plan_decrease_resource), "decrease", securities_);
    document["commitments"] = normalize_no_reduction_commitment_rows(
        rows(commitment_resource), securities_);
    document["insiders"] = normalize_insider_change_rows(
        rows(insider_resource), securities_);
    document["pledge_latest"] = normalize_pledge_latest_rows(
        rows(pledge_latest_resource), securities_);
    document["pledge_warning"] = normalize_pledge_risk_rows(
        rows(pledge_warning_resource), "warning", securities_);
    document["pledge_liquidation"] = normalize_pledge_risk_rows(
        rows(pledge_liquidation_resource), "liquidation", securities_);
    document["pledge_release"] = normalize_pledge_release_rows(
        rows(pledge_release_resource), securities_);
    document["change_monthly"] = normalize_ownership_statistics_rows(
        rows(change_month_resource), "month");
    document["change_annual"] = normalize_ownership_statistics_rows(
        rows(change_year_resource), "year");
    document["pledge_monthly"] = normalize_pledge_month_rows(
        rows(pledge_month_resource));
    document["institution_trust"] = normalize_pledge_institution_rows(
        rows(pledge_trust_resource), "trust");
    document["institution_broker"] = normalize_pledge_institution_rows(
        rows(pledge_broker_resource), "broker");

    Json summary = Json::object();
    summary["actual_changes"] = static_cast<std::uint64_t>(
        document.at("change_increase").size() + document.at("change_decrease").size());
    summary["increase_changes"] = static_cast<std::uint64_t>(
        document.at("change_increase").size());
    summary["decrease_changes"] = static_cast<std::uint64_t>(
        document.at("change_decrease").size());
    summary["plans"] = static_cast<std::uint64_t>(
        document.at("plan_increase").size() + document.at("plan_decrease").size());
    summary["insider_changes"] = static_cast<std::uint64_t>(
        document.at("insiders").size());
    summary["commitments"] = static_cast<std::uint64_t>(
        document.at("commitments").size());
    std::uint64_t insider_increases = 0, insider_decreases = 0;
    for (const auto& row : document.at("insiders").as_array())
        text_value(row, "direction") == "decrease"
            ? ++insider_decreases : ++insider_increases;
    summary["insider_increases"] = insider_increases;
    summary["insider_decreases"] = insider_decreases;
    std::uint64_t active_commitments = 0, upcoming_commitments = 0,
                  expired_commitments = 0;
    for (const auto& row : document.at("commitments").as_array()) {
        const auto status = text_value(row, "status");
        if (status == "active") ++active_commitments;
        else if (status == "upcoming") ++upcoming_commitments;
        else if (status == "expired") ++expired_commitments;
    }
    summary["active_commitments"] = active_commitments;
    summary["upcoming_commitments"] = upcoming_commitments;
    summary["expired_commitments"] = expired_commitments;
    summary["pledge_records"] = static_cast<std::uint64_t>(
        document.at("pledge_latest").size() +
        document.at("pledge_warning").size() +
        document.at("pledge_liquidation").size() +
        document.at("pledge_release").size());
    summary["pledge_latest"] = static_cast<std::uint64_t>(
        document.at("pledge_latest").size());
    summary["pledge_warning"] = static_cast<std::uint64_t>(
        document.at("pledge_warning").size());
    summary["pledge_liquidation"] = static_cast<std::uint64_t>(
        document.at("pledge_liquidation").size());
    summary["pledge_release"] = static_cast<std::uint64_t>(
        document.at("pledge_release").size());
    summary["institutions"] = static_cast<std::uint64_t>(
        document.at("institution_trust").size() +
        document.at("institution_broker").size());
    summary["unique_securities"] = unique_security_count({
        &document.at("change_increase"), &document.at("change_decrease"),
        &document.at("plan_increase"), &document.at("plan_decrease"),
        &document.at("insiders"), &document.at("commitments"),
        &document.at("pledge_latest"), &document.at("pledge_warning"),
        &document.at("pledge_liquidation"), &document.at("pledge_release")});
    document["summary"] = std::move(summary);

    Json sources = Json::array();
    for (const auto& resource : resources)
        sources.push_back(source_summary(document_for_resource(documents, resource)));
    document["sources"] = std::move(sources);
    core_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

OwnershipService::FetchResult OwnershipService::fetch_rankings(
    const OwnershipQuery& options) {
    const auto now = std::time(nullptr);
    const int age = rankings_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(
              0, now - rankings_cache_.fetched_at))
        : 0;
    if (!options.refresh && rankings_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {rankings_cache_.document, false, age};

    const std::vector<std::string> resources{
        ranking_increase_ratio_resource, ranking_increase_value_resource,
        ranking_increase_count_resource, ranking_decrease_ratio_resource,
        ranking_decrease_value_resource, ranking_decrease_count_resource};
    const auto documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);
    auto rows = [&](const char* resource) -> const Json& {
        return document_for_resource(documents, resource).at("rows");
    };

    Json document = Json::object();
    document["increase_ratio"] = normalize_ownership_ranking_rows(
        rows(ranking_increase_ratio_resource), "increase", "ratio", securities_);
    document["increase_value"] = normalize_ownership_ranking_rows(
        rows(ranking_increase_value_resource), "increase", "value", securities_);
    document["increase_count"] = normalize_ownership_ranking_rows(
        rows(ranking_increase_count_resource), "increase", "count", securities_);
    document["decrease_ratio"] = normalize_ownership_ranking_rows(
        rows(ranking_decrease_ratio_resource), "decrease", "ratio", securities_);
    document["decrease_value"] = normalize_ownership_ranking_rows(
        rows(ranking_decrease_value_resource), "decrease", "value", securities_);
    document["decrease_count"] = normalize_ownership_ranking_rows(
        rows(ranking_decrease_count_resource), "decrease", "count", securities_);

    Json summary = Json::object();
    summary["ranking_rows"] = static_cast<std::uint64_t>(
        document.at("increase_ratio").size() +
        document.at("increase_value").size() +
        document.at("increase_count").size() +
        document.at("decrease_ratio").size() +
        document.at("decrease_value").size() +
        document.at("decrease_count").size());
    summary["ranking_views"] = 6;
    summary["unique_securities"] = unique_security_count({
        &document.at("increase_ratio"), &document.at("increase_value"),
        &document.at("increase_count"), &document.at("decrease_ratio"),
        &document.at("decrease_value"), &document.at("decrease_count")});
    document["summary"] = std::move(summary);
    Json sources = Json::array();
    for (const auto& resource : resources)
        sources.push_back(source_summary(document_for_resource(documents, resource)));
    document["sources"] = std::move(sources);
    rankings_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

OwnershipService::FetchResult OwnershipService::fetch_shareholder_counts(
    const OwnershipQuery& options) {
    const auto now = std::time(nullptr);
    const int age = shareholder_counts_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(
              0, now - shareholder_counts_cache_.fetched_at))
        : 0;
    if (!options.refresh && shareholder_counts_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {shareholder_counts_cache_.document, false, age};

    const std::vector<std::string> resources{
        shareholder_sh_main_resource, shareholder_sz_main_resource,
        shareholder_chinext_resource, shareholder_star_resource,
        shareholder_bj_resource};
    const auto documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);
    auto rows = [&](const char* resource) -> const Json& {
        return document_for_resource(documents, resource).at("rows");
    };
    Json document = Json::object();
    document["sh_main"] = normalize_shareholder_count_rows(
        rows(shareholder_sh_main_resource), "sh-main", securities_);
    document["sz_main"] = normalize_shareholder_count_rows(
        rows(shareholder_sz_main_resource), "sz-main", securities_);
    document["sz_sme_legacy"] = Json::array();
    document["chinext"] = normalize_shareholder_count_rows(
        rows(shareholder_chinext_resource), "chinext", securities_);
    document["star"] = normalize_shareholder_count_rows(
        rows(shareholder_star_resource), "star", securities_);
    document["bj"] = normalize_shareholder_count_rows(
        rows(shareholder_bj_resource), "bj", securities_);

    Json summary = Json::object();
    summary["board_views"] = 6;
    summary["live_board_views"] = 5;
    summary["legacy_empty_views"] = 1;
    summary["shareholder_rows"] = static_cast<std::uint64_t>(
        document.at("sh_main").size() + document.at("sz_main").size() +
        document.at("chinext").size() + document.at("star").size() +
        document.at("bj").size());
    summary["unique_securities"] = unique_security_count({
        &document.at("sh_main"), &document.at("sz_main"),
        &document.at("chinext"), &document.at("star"), &document.at("bj")});
    document["summary"] = std::move(summary);
    Json sources = Json::array();
    sources.push_back(source_summary(document_for_resource(
        documents, shareholder_sh_main_resource)));
    sources.push_back(source_summary(document_for_resource(
        documents, shareholder_sz_main_resource)));
    sources.push_back(missing_source_summary(shareholder_sz_sme_legacy_resource));
    sources.push_back(source_summary(document_for_resource(
        documents, shareholder_chinext_resource)));
    sources.push_back(source_summary(document_for_resource(
        documents, shareholder_star_resource)));
    sources.push_back(source_summary(document_for_resource(
        documents, shareholder_bj_resource)));
    document["sources"] = std::move(sources);
    shareholder_counts_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

OwnershipService::FetchResult OwnershipService::fetch_resource(
    const std::string& resource, const OwnershipQuery& options) {
    const auto now = std::time(nullptr);
    for (auto item = resource_cache_.begin(); item != resource_cache_.end();) {
        if (now - item->second.fetched_at >= options.detail_cache_ttl_seconds)
            item = resource_cache_.erase(item);
        else ++item;
    }
    for (auto item = failure_cache_.begin(); item != failure_cache_.end();) {
        if (now - item->second.fetched_at >= options.detail_cache_ttl_seconds)
            item = failure_cache_.erase(item);
        else ++item;
    }
    if (options.refresh) {
        resource_cache_.erase(resource);
        failure_cache_.erase(resource);
    }
    const auto cached = resource_cache_.find(resource);
    if (cached != resource_cache_.end()) {
        const int age = static_cast<int>(std::max<std::time_t>(
            0, now - cached->second.fetched_at));
        return {cached->second.document, false, age};
    }
    const auto failure = failure_cache_.find(resource);
    if (failure != failure_cache_.end()) throw Error(failure->second.message);
    try {
        const auto source = fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
        resource_cache_[resource] = {source, std::time(nullptr)};
        return {source, true, 0};
    } catch (const std::exception& error) {
        failure_cache_[resource] = {error.what(), std::time(nullptr)};
        throw;
    }
}

}  // namespace tdx