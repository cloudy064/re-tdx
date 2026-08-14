#include "research_internal.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <set>

namespace tdx {

using namespace detail::research;

ResearchService::ResearchService(
    std::map<std::pair<int, std::string>, Security> securities,
    std::map<std::string, std::string> industry_names)
    : securities_(std::move(securities)), industry_names_(std::move(industry_names)) {}

ResearchService::FetchResult ResearchService::fetch_master(const ResearchQuery& options) {
    const auto now = std::time(nullptr);
    const int age = master_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - master_cache_.fetched_at)) : 0;
    if (!options.refresh && master_cache_.fetched_at && age < options.master_cache_ttl_seconds)
        return {master_cache_.document, false, age};

    std::vector<std::string> resources;
    for (const auto& category : research_categories()) resources.push_back(category.resource);
    const auto documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);
    Json document = Json::object(), categories = Json::array(), sources = Json::array();
    std::set<std::string> identities;
    std::uint64_t total_rows = 0;
    for (const auto& spec : research_categories()) {
        const auto& source = document_for_resource(documents, spec.resource);
        auto records = normalize_research_master_rows(
            source.at("rows"), spec.id, securities_, industry_names_);
        for (const auto& record : records.as_array())
            identities.insert(text_value(record.at("entity"), "type") + ":" +
                              text_value(record.at("entity"), "id"));
        Json category = Json::object();
        category["id"] = spec.id;
        category["label"] = spec.label;
        category["entity_type"] = spec.entity_type;
        category["resource"] = spec.resource;
        category["record_count"] = static_cast<std::uint64_t>(records.size());
        category["records"] = std::move(records);
        total_rows += static_cast<std::uint64_t>(category.at("record_count").as_number());
        categories.push_back(std::move(category));
        sources.push_back(source_summary(source));
    }
    document["categories"] = std::move(categories);
    document["sources"] = std::move(sources);
    document["total_rows"] = total_rows;
    document["unique_entities"] = static_cast<std::uint64_t>(identities.size());
    master_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

ResearchService::FetchResult ResearchService::fetch_detail(
    const std::string& resource, const ResearchQuery& options) {
    const auto now = std::time(nullptr);
    for (auto item = detail_cache_.begin(); item != detail_cache_.end();) {
        if (now - item->second.fetched_at >= options.detail_cache_ttl_seconds)
            item = detail_cache_.erase(item);
        else
            ++item;
    }
    const auto cached = detail_cache_.find(resource);
    const int age = cached == detail_cache_.end() ? 0
        : static_cast<int>(std::max<std::time_t>(0, now - cached->second.fetched_at));
    if (!options.refresh && cached != detail_cache_.end())
        return {cached->second.document, false, age};
    const auto source = fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
    detail_cache_[resource] = {source, std::time(nullptr)};
    return {source, true, 0};
}

}  // namespace tdx
