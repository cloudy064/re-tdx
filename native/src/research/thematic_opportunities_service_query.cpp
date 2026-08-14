#include "thematic_opportunities_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

namespace tdx {

Json ThematicOpportunityService::query(const ThematicOpportunityQuery& input) {
    using namespace detail::thematic_opportunities;
    ThematicOpportunityQuery options = input;
    options.view = lower_ascii(trim(options.view));
    options.type = lower_ascii(trim(options.type));
    options.group_id = trim(options.group_id);
    options.market = lower_ascii(trim(options.market));
    options.code = trim(options.code);
    options.query = lower_ascii(trim(options.query));
    options.sort = lower_ascii(trim(options.sort));
    options.order = lower_ascii(trim(options.order));
    if (!valid_view(options.view))
        throw Error("view must be catalog, groups, group, security, hype-completed, or hype-active");
    if (!valid_type(options.type))
        throw Error("type must be all, industry, region, or legacy-client-theme");
    if (options.view == "group" && options.group_id.empty())
        throw Error("group view requires group_id");
    if (options.view == "security" &&
        (options.market.empty() || !digits(options.code, 6)))
        throw Error("security view requires market and six-digit code");
    if (options.offset < 0 || options.offset > 1000000 ||
        options.limit < 1 || options.limit > 5000)
        throw Error("offset/limit is outside the supported range");
    if (options.master_cache_ttl_seconds < 0 ||
        options.master_cache_ttl_seconds > 86400 ||
        options.detail_cache_ttl_seconds < 0 ||
        options.detail_cache_ttl_seconds > 86400)
        throw Error("cache TTL is outside the supported range");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");

    const auto master = fetch_master(options);
    Json groups = Json::array(), details = Json::array(), completed = Json::array(),
         active = Json::array(), selected_group = Json(nullptr), errors = Json::array();
    Json sources = master.document.at("sources");
    bool detail_refreshed = false;
    int detail_age = 0;
    const int selected_market = options.view == "security"
        ? market_id(options.market) : -1;
    for (const auto& group : master.document.at("groups").as_array()) {
        bool keep = options.type == "all" || group.at("type").as_string() == options.type;
        if (keep && !options.query.empty() &&
            !json_contains(group_summary(group), options.query)) keep = false;
        if (keep && options.view == "security") {
            keep = false;
            for (const auto& member : group.at("members").as_array())
                if (static_cast<int>(member.at("market_id").as_number()) == selected_market &&
                    member.at("code").as_string() == options.code) {
                    keep = true;
                    break;
                }
        }
        if (keep) groups.push_back(group_summary(group));
        if (options.view == "group" &&
            group.at("group_id").as_string() == options.group_id)
            selected_group = group;
    }
    if (options.view == "group" && selected_group.is_null())
        throw Error("group_id is absent from the active thematic opportunity catalog");
    sort_groups(groups, options.sort, options.order);

    std::uint64_t matched = groups.size();
    if (options.view == "catalog" || options.view == "groups" ||
        options.view == "security") {
        groups = page(groups, options.offset, options.limit);
    } else if (options.view == "group") {
        matched = static_cast<std::uint64_t>(selected_group.at("member_count").as_number());
        if (options.include_detail) {
            try {
                const auto detail_resource =
                    selected_group.at("detail_resource").as_string();
                const auto fetched = fetch_detail(detail_resource, options);
                detail_refreshed = fetched.refreshed;
                detail_age = fetched.age_seconds;
                sources.push_back(jsn_source_metadata(fetched.document));
                details = selected_group.at("type").as_string() ==
                        "legacy-client-theme"
                    ? normalize_legacy_client_theme_details(
                        fetched.document.at("rows"), securities_)
                    : normalize_opportunity_group_details(
                        fetched.document.at("rows"), securities_);
                matched = details.size();
            } catch (const std::exception& error) {
                Json failure = Json::object();
                failure["resource"] = selected_group.at("detail_resource");
                failure["message"] = error.what();
                errors.push_back(std::move(failure));
            }
        }
        selected_group = group_summary(selected_group);
        selected_group["detail_available"] = details.size() > 0;
        details = page(details, options.offset, options.limit);
        groups = Json::array();
    } else if (options.view == "hype-completed") {
        completed = master.document.at("completed_hype");
        if (!options.query.empty()) {
            Json filtered = Json::array();
            for (const auto& row : completed.as_array())
                if (json_contains(row, options.query)) filtered.push_back(row);
            completed = std::move(filtered);
        }
        sort_hype(completed, options.order);
        matched = completed.size();
        completed = page(completed, options.offset, options.limit);
        groups = Json::array();
    } else {
        active = master.document.at("active_hype");
        if (!options.query.empty()) {
            Json filtered = Json::array();
            for (const auto& row : active.as_array())
                if (json_contains(row, options.query)) filtered.push_back(row);
            active = std::move(filtered);
        }
        sort_hype(active, options.order);
        matched = active.size();
        active = page(active, options.offset, options.limit);
        groups = Json::array();
    }

    const auto health = jsn_sources_health(sources);
    Json result = Json::object();
    result["schema"] = "tdx-market-thematic-opportunities-native-v1";
    result["generated_at"] = current_time_text();
    result["view"] = options.view;
    result["availability"] = health.at("stale").as_bool() ? "stale-cache" :
        errors.size() ? "partial" : matched ? "live" : "empty";
    result["groups"] = std::move(groups);
    result["selected_group"] = std::move(selected_group);
    result["details"] = std::move(details);
    result["completed_hype"] = std::move(completed);
    result["active_hype"] = std::move(active);
    result["summary"] = master.document.at("summary");
    result["errors"] = std::move(errors);
    Json counts = Json::object();
    counts["matched"] = matched;
    counts["returned_groups"] = static_cast<std::uint64_t>(result.at("groups").size());
    counts["returned_details"] = static_cast<std::uint64_t>(result.at("details").size());
    counts["returned_completed_hype"] =
        static_cast<std::uint64_t>(result.at("completed_hype").size());
    counts["returned_active_hype"] =
        static_cast<std::uint64_t>(result.at("active_hype").size());
    result["counts"] = std::move(counts);
    Json filters = Json::object();
    filters["type"] = options.type;
    filters["group_id"] = options.group_id.empty() ? Json(nullptr) : Json(options.group_id);
    filters["market"] = options.market.empty() ? Json(nullptr) : Json(options.market);
    filters["code"] = options.code.empty() ? Json(nullptr) : Json(options.code);
    filters["query"] = options.query;
    filters["sort"] = options.sort;
    filters["order"] = options.order;
    filters["offset"] = options.offset;
    filters["limit"] = options.limit;
    result["filters"] = std::move(filters);
    result["sources"] = std::move(sources);
    result["upstream_health"] = health;
    Json cache = Json::object();
    cache["master_refreshed"] = master.refreshed;
    cache["master_age_seconds"] = master.age_seconds;
    cache["detail_refreshed"] = detail_refreshed;
    cache["detail_age_seconds"] = detail_age;
    result["cache"] = std::move(cache);
    result["semantics"] =
        "YDYL is the client Belt-and-Road industry/region hierarchy: master $ZQDM is a group id and ydyl1/<group-id> contains per-security investment logic. XNXS is retained as a legacy-client-theme source because its page declares virtual reality while the current upstream row is fast-neutron reactor; the row-level name wins and semantic_mismatch remains explicit. JSYSP/NZJSP and XNXS fqprice/price fields are reference closes, not returns; current quote is intentionally not fabricated. RDHS101 is the completed recent-hype review by block and leader, while RDHS102 is the currently active-hype security list with stock and Shanghai-index interval returns.";
    return result;
}

}  // namespace tdx
