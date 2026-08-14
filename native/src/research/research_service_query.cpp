#include "research_internal.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>

namespace tdx {
namespace {

using namespace detail::research;

struct DetailProjection {
    const char* target;
    Json rows;
};

DetailProjection normalize_detail(
    DetailKind kind, const Json& rows, bool include_text,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    switch (kind) {
        case DetailKind::activity:
            return {"activities", normalize_research_activity_rows(rows, include_text)};
        case DetailKind::regulatory:
            return {"regulatory_events",
                    normalize_research_regulatory_rows(rows, include_text)};
        case DetailKind::related_security:
            return {"related_securities", normalize_research_security_rows(rows, securities)};
    }
    throw Error("unsupported research detail kind");
}

void validate_query(const ResearchQuery& options, const ResearchCategorySpec& spec) {
    if (options.limit < 1 || options.limit > 5000) throw Error("limit must be in 1..5000");
    if (options.detail_limit < 1 || options.detail_limit > 1000)
        throw Error("detail_limit must be in 1..1000");
    if (options.master_cache_ttl_seconds < 0 || options.master_cache_ttl_seconds > 86400 ||
        options.detail_cache_ttl_seconds < 0 || options.detail_cache_ttl_seconds > 86400)
        throw Error("research cache TTL is outside the supported range");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    const bool security_mode = !options.market.empty() || !options.code.empty();
    if (security_mode && (options.market.empty() || options.code.empty()))
        throw Error("security selection requires both market and code");
    if (security_mode && !six_digits(options.code)) throw Error("code must contain six digits");
    if (security_mode && spec.entity_type != "security")
        throw Error("market/code selection is only valid for a security category");
}

Json catalog_projection(const Json& master) {
    Json result = Json::array();
    for (const auto& category : master.at("categories").as_array()) {
        Json item = Json::object();
        for (const auto* key : {"id", "label", "entity_type", "resource", "record_count"})
            item[key] = category.at(key);
        result.push_back(std::move(item));
    }
    return result;
}

}  // namespace

Json ResearchService::query(const ResearchQuery& options) {
    using namespace detail::research;
    const auto& spec = category_spec(options.category);
    validate_query(options, spec);
    const bool security_mode = !options.market.empty() || !options.code.empty();
    auto master = fetch_master(options);
    const auto& selected_category = category_document(master.document, options.category);
    Json records = Json::array();
    const int selected_market = security_mode ? canonical_market_id(options.market) : 0;
    for (const auto& record : selected_category.at("records").as_array()) {
        if (security_mode && !same_security(record, selected_market, options.code)) continue;
        if (!options.detail_id.empty() &&
            text_value(record, "detail_id") != options.detail_id &&
            text_value(record.at("entity"), "id") != options.detail_id) continue;
        if (!options.query.empty() && !json_contains(record, lower_ascii(options.query))) continue;
        if (static_cast<int>(records.size()) >= options.limit) break;
        records.push_back(record);
    }

    const bool detail_mode = security_mode || !options.detail_id.empty() || options.include_details;
    Json result = Json::object();
    result["schema"] = "tdx-market-research-native-v1";
    result["generated_at"] = now_text();
    result["mode"] = security_mode ? "security" : detail_mode ? "detail" : "master";
    result["category"] = options.category;
    result["category_label"] = spec.label;
    result["entity_type"] = spec.entity_type;
    result["query"] = options.query;
    result["categories"] = catalog_projection(master.document);
    result["records"] = std::move(records);
    const Json* selected_record = result.at("records").size()
        ? &result.at("records").as_array().front() : nullptr;
    result["selected"] = selected_record ? *selected_record : Json(nullptr);
    result["selected_entity"] = security_mode
        ? security_document(selected_market, options.code, securities_)
        : selected_record ? selected_record->at("entity") : Json(nullptr);
    result["category_memberships"] = Json::array();
    if (security_mode) {
        for (const auto& category : master.document.at("categories").as_array()) {
            if (text_value(category, "entity_type") != "security") continue;
            for (const auto& record : category.at("records").as_array()) {
                if (!same_security(record, selected_market, options.code)) continue;
                Json membership = Json::object();
                membership["category"] = category.at("id");
                membership["category_label"] = category.at("label");
                membership["record"] = record;
                result["category_memberships"].push_back(std::move(membership));
            }
        }
    }
    result["activities"] = Json::array();
    result["regulatory_events"] = Json::array();
    result["related_securities"] = Json::array();
    result["detail_errors"] = Json::array();
    result["sources"] = master.document.at("sources");

    Json cache = Json::object();
    cache["master_ttl_seconds"] = options.master_cache_ttl_seconds;
    cache["master_refreshed"] = master.refreshed;
    cache["master_age_seconds"] = master.age_seconds;
    bool any_detail_refreshed = false;
    int greatest_detail_age = 0;
    std::string detail_id = options.detail_id;
    if (detail_id.empty() && selected_record) detail_id = text_value(*selected_record, "detail_id");
    if (detail_id.empty() && security_mode)
        detail_id = derived_detail_id(options.category, options.code);

    if (detail_mode) {
        if (detail_id.empty()) {
            Json failure = Json::object();
            failure["resource"] = Json(nullptr);
            failure["message"] = "selected row does not expose a dynamic detail key";
            result["detail_errors"].push_back(std::move(failure));
        } else {
            for (const auto& name_space : spec.detail_namespaces) {
                const auto resource = name_space + "/" + detail_id + ".jsn";
                try {
                    auto detail = fetch_detail(resource, options);
                    any_detail_refreshed = any_detail_refreshed || detail.refreshed;
                    greatest_detail_age = std::max(greatest_detail_age, detail.age_seconds);
                    result["sources"].push_back(source_summary(detail.document));
                    auto projection = normalize_detail(
                        detail_kind(name_space), detail.document.at("rows"),
                        options.include_text, securities_);
                    result[projection.target] = limited_rows(projection.rows, options.detail_limit);
                } catch (const std::exception& error) {
                    Json failure = Json::object();
                    failure["resource"] = resource;
                    failure["message"] = error.what();
                    result["detail_errors"].push_back(std::move(failure));
                }
            }
        }
        cache["detail_ttl_seconds"] = options.detail_cache_ttl_seconds;
        cache["detail_refreshed"] = any_detail_refreshed;
        cache["detail_age_seconds"] = greatest_detail_age;
        cache["detail_id"] = detail_id;
    }

    Json counts = Json::object();
    counts["categories"] = static_cast<std::uint64_t>(master.document.at("categories").size());
    counts["total_rows"] = master.document.at("total_rows");
    counts["unique_entities"] = master.document.at("unique_entities");
    counts["category_rows"] = selected_category.at("record_count");
    counts["returned_records"] = static_cast<std::uint64_t>(result.at("records").size());
    counts["memberships"] = static_cast<std::uint64_t>(result.at("category_memberships").size());
    counts["activities"] = static_cast<std::uint64_t>(result.at("activities").size());
    counts["regulatory_events"] = static_cast<std::uint64_t>(result.at("regulatory_events").size());
    counts["related_securities"] = static_cast<std::uint64_t>(result.at("related_securities").size());
    counts["detail_errors"] = static_cast<std::uint64_t>(result.at("detail_errors").size());
    result["counts"] = std::move(counts);
    const auto upstream_health = jsn_sources_health(result.at("sources"));
    const bool stale = upstream_health.at("stale").as_bool();
    result["availability"] = stale ? "stale-cache" :
        result.at("detail_errors").size() ? "partial" : "live";
    cache["stale"] = stale;
    cache["upstream"] = upstream_health;
    result["cache"] = std::move(cache);
    return result;
}

}  // namespace tdx
