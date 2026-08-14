#include "industry_profile_internal.hpp"

#include "tdx/jsn.hpp"

#include <algorithm>
#include <ctime>
#include <functional>
#include <map>

namespace tdx {

IndustryProfileService::FetchResult IndustryProfileService::fetch_master(
    const IndustryProfileQuery& options) {
    const auto now = std::time(nullptr);
    const int age = master_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - master_cache_.fetched_at)) : 0;
    if (!options.refresh && master_cache_.fetched_at && age < options.master_cache_ttl_seconds)
        return {master_cache_.document, false, age};

    const auto& resources = detail::industry_profile::industry_master_resource_paths();
    const auto documents = fetch_jsn_resources_rows(
        resources, "bi", options.timeout_ms);
    std::map<std::string, Json> holdings_by_code;
    Json sources = Json::array();
    for (const auto& period : detail::industry_profile::industry_holding_periods()) {
        const auto& source = detail::industry_profile::find_resource_document(documents, period.resource);
        const auto normalized = normalize_industry_holdings_rows(
            source.at("rows"), std::string(period.key),
            std::string(period.label), blocks_);
        for (const auto& item : normalized.as_array()) {
            auto& records = holdings_by_code[detail::industry_profile::json_text(item.at("industry"), "code")];
            if (!records.is_array()) records = Json::array();
            records.push_back(item);
        }
        sources.push_back(detail::industry_profile::resource_source_summary(source));
    }
    const auto& shareholder_source = detail::industry_profile::find_resource_document(
        documents, detail::industry_profile::industry_shareholder_resource());
    const auto shareholder_rows = normalize_industry_shareholder_rows(
        shareholder_source.at("rows"), blocks_);
    std::map<std::string, Json> shareholders_by_code;
    for (const auto& item : shareholder_rows.as_array())
        shareholders_by_code[detail::industry_profile::json_text(item.at("industry"), "code")] = item;
    sources.push_back(detail::industry_profile::resource_source_summary(shareholder_source));

    std::map<std::string, Json> nodes;
    Json industries = Json::array();
    for (const auto& [code, block] : blocks_) {
        Json item = detail::industry_profile::make_basic_industry(block, parents_[code]);
        const auto holdings = holdings_by_code.find(code);
        item["has_holdings"] = holdings != holdings_by_code.end();
        item["holdings_periods"] = holdings == holdings_by_code.end()
            ? Json::array() : holdings->second;
        const auto shareholder = shareholders_by_code.find(code);
        item["has_shareholder_profile"] = shareholder != shareholders_by_code.end();
        item["shareholder_profile"] = shareholder == shareholders_by_code.end()
            ? Json(nullptr) : shareholder->second;
        nodes[code] = item;
        industries.push_back(std::move(item));
    }

    std::function<Json(const std::string&)> build_tree = [&](const std::string& code) {
        Json node = nodes.at(code);
        Json children = Json::array();
        const auto found = children_.find(code);
        if (found != children_.end())
            for (const auto& child : found->second) children.push_back(build_tree(child));
        node["children"] = std::move(children);
        return node;
    };
    Json tree = Json::array();
    for (const auto& [code, block] : blocks_)
        if (parents_[code].empty()) tree.push_back(build_tree(code));

    Json counts = Json::object();
    counts["industries"] = static_cast<std::uint64_t>(blocks_.size());
    counts["holding_industries"] = static_cast<std::uint64_t>(holdings_by_code.size());
    counts["shareholder_industries"] =
        static_cast<std::uint64_t>(shareholders_by_code.size());
    counts["holding_master_rows"] =
        static_cast<std::uint64_t>(detail::industry_profile::industry_holding_periods().size() * holdings_by_code.size());
    counts["shareholder_master_rows"] =
        static_cast<std::uint64_t>(shareholders_by_code.size());
    Json document = Json::object();
    document["schema"] = "tdx-industry-profile-native-v1";
    document["industries"] = std::move(industries);
    document["tree"] = std::move(tree);
    document["sources"] = std::move(sources);
    document["counts"] = std::move(counts);
    master_cache_ = {document, now};
    return {std::move(document), true, 0};
}

IndustryProfileService::FetchResult IndustryProfileService::fetch_detail(
    const std::string& resource, const IndustryProfileQuery& options) {
    const auto now = std::time(nullptr);
    for (auto item = detail_cache_.begin(); item != detail_cache_.end();) {
        if (now - item->second.fetched_at >= options.detail_cache_ttl_seconds)
            item = detail_cache_.erase(item);
        else ++item;
    }
    const auto cached = detail_cache_.find(resource);
    const int age = cached == detail_cache_.end() ? 0
        : static_cast<int>(std::max<std::time_t>(0, now - cached->second.fetched_at));
    if (!options.refresh && cached != detail_cache_.end())
        return {cached->second.document, false, age};
    const auto source = fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
    detail_cache_[resource] = {source, now};
    return {source, true, 0};
}

}  // namespace tdx

