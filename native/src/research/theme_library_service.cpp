#include "tdx/theme_library_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <set>
#include <string>
#include <utility>

namespace tdx {

using namespace tdx::theme_library_detail;

namespace {

// Trims and lower-cases the free-text options, then rejects unsupported views,
// sources, missing view-specific keys and out-of-range paging.
void validate_query(ThemeLibraryQuery& options) {
    options.view = lower_ascii(trim(options.view));
    options.source = lower_ascii(trim(options.source));
    options.theme_id = trim(options.theme_id);
    options.market = lower_ascii(trim(options.market));
    options.code = trim(options.code);
    options.query = trim(options.query);
    options.sort = lower_ascii(trim(options.sort));
    options.order = lower_ascii(trim(options.order));
    if (!std::set<std::string>{"catalog", "theme", "security"}.count(options.view))
        throw Error("view must be catalog, theme, or security");
    std::set<std::string> source_ids{"all"};
    for (const auto& source : theme_library_sources()) source_ids.insert(source.id);
    if (!source_ids.count(options.source))
        throw Error("unknown theme-library source: " + options.source);
    if (options.view == "theme" && options.theme_id.empty())
        throw Error("theme view requires theme_id");
    if (options.view == "security" &&
        (options.market.empty() || !digits(options.code, 6)))
        throw Error("security view requires market and six-digit code");
    if (options.offset < 0 || options.offset > 1000000 ||
        options.limit < 1 || options.limit > 5000)
        throw Error("theme-library paging is outside the supported range");
}

// One pass over the master snapshots: builds the filtered summary list and, for
// the theme view, captures the full record (members and raw included).
void filter_and_select(const Json& themes, const ThemeLibraryQuery& options,
                       Json& filtered, Json& selected) {
    const auto needle = lower_ascii(options.query);
    const int selected_market =
        options.view == "security" ? market_id(options.market) : -1;
    for (const auto& theme : themes.as_array()) {
        bool keep = options.source == "all" ||
                    theme.at("source").as_string() == options.source;
        if (keep && !needle.empty() && !json_contains(theme_summary(theme), needle))
            keep = false;
        if (keep && options.view == "security") {
            keep = false;
            for (const auto& member : theme.at("members").as_array())
                if (static_cast<int>(member.at("market_id").as_number()) == selected_market &&
                    member.at("code").as_string() == options.code) {
                    keep = true;
                    break;
                }
        }
        if (keep) filtered.push_back(theme_summary(theme));
        if (options.view == "theme" &&
            theme.at("theme_id").as_string() == options.theme_id &&
            (options.source == "all" ||
             theme.at("source").as_string() == options.source))
            selected = theme;
    }
}

Json page_array(const Json& source, int offset, int limit) {
    Json result = Json::array();
    for (std::size_t i = static_cast<std::size_t>(offset);
         i < source.size() && result.size() < static_cast<std::size_t>(limit); ++i)
        result.push_back(source.as_array()[i]);
    return result;
}

void record_failure(Json& errors, const Json& resource, const char* message) {
    Json failure = Json::object();
    failure["resource"] = resource;
    failure["message"] = message;
    errors.push_back(std::move(failure));
}

}  // namespace

Json ThemeLibraryService::query(const ThemeLibraryQuery& input) {
    ThemeLibraryQuery options = input;
    validate_query(options);

    const auto master = fetch_master(options);
    Json filtered = Json::array(), selected = Json(nullptr), members = Json::array();
    Json details = Json::array(), chart = Json::array(), errors = Json::array();
    Json sources = master.document.at("sources");
    filter_and_select(master.document.at("themes"), options, filtered, selected);
    if (options.view == "theme" && selected.is_null())
        throw Error("theme_id is absent from the selected theme-library source");
    sort_themes(filtered, options.sort, options.order);
    const auto matched = static_cast<std::uint64_t>(filtered.size());
    Json themes = page_array(filtered, options.offset, options.limit);

    bool detail_refreshed = false, chart_refreshed = false;
    int detail_age = 0, chart_age = 0;
    if (options.view == "theme") {
        // A non-empty dynamic detail supersedes the master snapshot's members.
        Json active = selected.at("members");
        if (options.include_detail) {
            try {
                const auto resource = selected.at("detail_resource").as_string();
                const auto fetched = fetch_dynamic(resource, options);
                detail_refreshed = fetched.refreshed;
                detail_age = fetched.age_seconds;
                sources.push_back(jsn_source_metadata(fetched.document));
                details = normalize_theme_library_details(
                    fetched.document.at("rows"), blocks_.securities);
                if (details.size()) {
                    active = Json::array();
                    for (const auto& row : details.as_array())
                        active.push_back(row.at("security"));
                }
            } catch (const std::exception& error) {
                record_failure(errors, selected.at("detail_resource"), error.what());
            }
        }
        if (options.include_chart) {
            try {
                const auto resource = selected.at("chart_resource").as_string();
                const auto fetched = fetch_dynamic(resource, options);
                chart_refreshed = fetched.refreshed;
                chart_age = fetched.age_seconds;
                sources.push_back(jsn_source_metadata(fetched.document));
                chart = normalize_theme_library_chart(fetched.document.at("rows"));
            } catch (const std::exception& error) {
                record_failure(errors, selected.at("chart_resource"), error.what());
            }
        }
        const auto active_count = static_cast<std::uint64_t>(active.size());
        members = page_array(active, options.offset, options.limit);
        details = page_array(details, options.offset, options.limit);
        selected = theme_summary(selected);
        selected["active_member_count"] = active_count;
        selected["detail_available"] = details.size() > 0;
        selected["chart_available"] = chart.size() > 0;
    }

    const auto health = jsn_sources_health(sources);
    Json result = Json::object();
    result["schema"] = "tdx-market-theme-library-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["availability"] = health.at("stale").as_bool() ? "stale-cache" :
        errors.size() ? "partial" :
        ((options.view == "theme" ? !selected.is_null() : matched > 0) ? "live" : "empty");
    result["source_options"] = master.document.at("source_options");
    result["themes"] = std::move(themes);
    result["selected_theme"] = std::move(selected);
    result["members"] = std::move(members);
    result["details"] = std::move(details);
    result["chart"] = std::move(chart);
    result["summary"] = master.document.at("summary");
    Json counts = Json::object();
    counts["matched"] = matched;
    counts["returned_themes"] = static_cast<std::uint64_t>(result.at("themes").size());
    counts["returned_members"] = static_cast<std::uint64_t>(result.at("members").size());
    counts["returned_details"] = static_cast<std::uint64_t>(result.at("details").size());
    counts["chart_points"] = static_cast<std::uint64_t>(result.at("chart").size());
    result["counts"] = std::move(counts);
    result["errors"] = std::move(errors);
    result["sources"] = std::move(sources);
    result["upstream_health"] = health;
    Json cache = Json::object();
    cache["master_refreshed"] = master.refreshed;
    cache["master_age_seconds"] = master.age_seconds;
    cache["detail_refreshed"] = detail_refreshed;
    cache["detail_age_seconds"] = detail_age;
    cache["chart_refreshed"] = chart_refreshed;
    cache["chart_age_seconds"] = chart_age;
    result["cache"] = std::move(cache);
    result["semantics"] =
        "The five ZTTZ masters are source snapshots, not a strict parent-child tree. "
        "Overlapping ids may carry different membership snapshots, so record_id is SOURCE:ID. "
        "The general source is the broad client catalog; zttz/ID supplies current per-security "
        "inclusion descriptions and zttz1/ID supplies the theme index history. A non-empty dynamic "
        "detail supersedes the selected master snapshot's member list.";
    return result;
}

}  // namespace tdx
