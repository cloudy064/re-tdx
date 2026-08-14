#include "strategic_themes_internal.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

namespace fs = std::filesystem;

namespace tdx {

StrategicThemeService::StrategicThemeService(
    fs::path root, std::map<std::pair<int, std::string>, Security> securities)
    : root_(std::move(root)) {
    blocks_.securities = std::move(securities);
}

Json StrategicThemeService::query(const StrategicThemeQuery& input) {
    using namespace detail::strategic_themes;
    const auto plan = make_query_plan(input);
    const auto& options = plan.options;
    const auto master = fetch_master(options);
    Json categories = Json::array();
    Json themes = Json::array();
    Json members = Json::array();
    Json details = Json::array();
    Json selected_theme = Json(nullptr);
    Json errors = Json::array();
    Json sources = master.document.at("sources");
    const auto needle = lower_ascii(options.query);
    std::uint64_t matched = 0;
    bool detail_refreshed = false;
    int detail_age = 0;

    for (const auto& category : master.document.at("categories").as_array()) {
        const bool selected = plan.view->kind != ViewKind::categories ||
            options.category.empty() ||
            category.at("block_id").as_string() == options.category ||
            category.at("name").as_string() == options.category;
        const bool query_matches = plan.view->kind != ViewKind::categories ||
            needle.empty() || json_contains(category, needle);
        if (selected && query_matches) categories.push_back(category);
    }

    Json filtered_themes = Json::array();
    for (const auto& theme : master.document.at("themes").as_array()) {
        bool keep = true;
        if (!options.category.empty()) {
            keep = false;
            for (std::size_t index = 0; index < theme.at("categories").size(); ++index)
                if (theme.at("categories").as_array()[index].as_string() ==
                        options.category ||
                    theme.at("category_block_ids").as_array()[index].as_string() ==
                        options.category) {
                    keep = true;
                    break;
                }
        }
        if (keep && plan.view->kind == ViewKind::security) {
            keep = false;
            for (const auto& member : theme.at("members").as_array())
                if (static_cast<int>(member.at("market_id").as_number()) ==
                        plan.selected_market &&
                    member.at("code").as_string() == options.code) {
                    keep = true;
                    break;
                }
        }
        if (keep && !needle.empty() && !json_contains(theme_summary(theme), needle))
            keep = false;
        if (keep) filtered_themes.push_back(theme_summary(theme));
        if (plan.view->kind == ViewKind::theme &&
            theme.at("theme_id").as_string() == options.theme_id)
            selected_theme = theme;
    }
    if (plan.view->kind == ViewKind::theme && selected_theme.is_null())
        throw Error("theme_id is absent from the active strategic-theme catalog");
    sort_themes(filtered_themes, *plan.sort, options.order);
    matched = filtered_themes.size();

    if (plan.view->kind == ViewKind::catalog ||
        plan.view->kind == ViewKind::themes ||
        plan.view->kind == ViewKind::security) {
        for (std::size_t index = static_cast<std::size_t>(options.offset);
             index < filtered_themes.size() &&
                 themes.size() < static_cast<std::size_t>(options.limit);
             ++index)
            themes.push_back(filtered_themes.as_array()[index]);
    } else if (plan.view->kind == ViewKind::categories) {
        matched = categories.size();
        Json paged = Json::array();
        for (std::size_t index = static_cast<std::size_t>(options.offset);
             index < categories.size() &&
                 paged.size() < static_cast<std::size_t>(options.limit);
             ++index)
            paged.push_back(categories.as_array()[index]);
        categories = std::move(paged);
    } else {
        matched = static_cast<std::uint64_t>(
            selected_theme.at("master_member_count").as_number());
        Json active = selected_theme.at("members");
        if (options.include_detail) {
            try {
                const auto fetched = fetch_detail(options.theme_id, options);
                detail_refreshed = fetched.refreshed;
                detail_age = fetched.age_seconds;
                sources.push_back(jsn_source_metadata(fetched.document));
                details = normalize_strategic_theme_details(
                    fetched.document.at("rows"), blocks_.securities);
                if (details.size()) {
                    active = Json::array();
                    for (const auto& detail : details.as_array())
                        active.push_back(detail.at("security"));
                }
            } catch (const std::exception& error) {
                Json failure = Json::object();
                failure["resource"] = selected_theme.at("detail_resource");
                failure["message"] = error.what();
                errors.push_back(std::move(failure));
            }
        }
        matched = active.size();
        const bool detail_available = details.size() > 0;
        Json paged_details = Json::array();
        for (std::size_t index = static_cast<std::size_t>(options.offset);
             index < active.size() &&
                 members.size() < static_cast<std::size_t>(options.limit);
             ++index) {
            members.push_back(active.as_array()[index]);
            if (details.size()) paged_details.push_back(details.as_array()[index]);
        }
        details = std::move(paged_details);
        selected_theme = theme_summary(selected_theme);
        selected_theme["detail_available"] = detail_available;
        selected_theme["member_count"] = matched;
    }

    Json summary = master.document.at("summary");
    if (plan.view->kind == ViewKind::security)
        summary["selected_security_theme_count"] = matched;
    const auto health = jsn_sources_health(sources);
    Json result = Json::object();
    result["schema"] = "tdx-market-strategic-themes-native-v1";
    result["generated_at"] = now_text();
    result["view"] = plan.view->id;
    result["availability"] = health.at("stale").as_bool() ? "stale-cache" :
        errors.size() ? "partial" : matched ? "live" : "empty";
    result["categories"] = std::move(categories);
    result["themes"] = std::move(themes);
    result["selected_theme"] = std::move(selected_theme);
    result["members"] = std::move(members);
    result["details"] = std::move(details);
    result["summary"] = std::move(summary);
    result["errors"] = std::move(errors);
    Json counts = Json::object();
    counts["matched"] = matched;
    counts["returned_categories"] = static_cast<std::uint64_t>(
        result.at("categories").size());
    counts["returned_themes"] = static_cast<std::uint64_t>(
        result.at("themes").size());
    counts["returned_members"] = static_cast<std::uint64_t>(
        result.at("members").size());
    counts["returned_details"] = static_cast<std::uint64_t>(
        result.at("details").size());
    result["counts"] = std::move(counts);
    Json filters = Json::object();
    filters["category"] = options.category.empty()
        ? Json(nullptr) : Json(options.category);
    filters["theme_id"] = options.theme_id.empty()
        ? Json(nullptr) : Json(options.theme_id);
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
        "The 26 strategic-theme pages form a category -> internal theme id -> security hierarchy. The master $ZQDM is a theme id, not a stock code; zttzty/<source-theme-id> contains per-security inclusion logic. The legacy Internet+ catalog is namespaced with HLW: because some ids carry a category-specific membership snapshot. Duplicate unnamespaced themes across categories must have identical names and master memberships. A non-empty detail resource supersedes the master member list for that selected theme.";
    return result;
}

}  // namespace tdx
