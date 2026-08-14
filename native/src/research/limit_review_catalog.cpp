#include "limit_review_internal.hpp"

#include <algorithm>

namespace tdx::detail::limit_review {
namespace {

constexpr std::array<ResourceSpec, 5> resources{{
    {"list/func_zdtfx101_1.jsn", NormalizeKind::current_limit_up},
    {"list/func_zdtfx102_1.jsn", NormalizeKind::current_limit_down},
    {"list/func_zdtfx103_1.jsn", NormalizeKind::current_surge},
    {"list/func_zdtfx106_1.jsn", NormalizeKind::annual},
    {"list/func_zdtfx107_1.jsn", NormalizeKind::market_history},
}};

constexpr std::array<ViewSpec, 6> views{{
    {"catalog", ViewKind::catalog, "目录", "[]", "[]",
     "Typed limit-review capability catalog."},
    {"current", ViewKind::current, "当日涨跌停复盘",
     "[\"all\",\"limit-up\",\"limit-down\",\"surge\"]",
     "[\"list/func_zdtfx101_1.jsn\",\"list/func_zdtfx102_1.jsn\",\"list/func_zdtfx103_1.jsn\"]",
     "Progressively updated non-realtime review tables, not the live order book."},
    {"annual", ViewKind::annual, "个股年度涨跌停行为", "[\"all\"]",
     "[\"list/func_zdtfx106_1.jsn\"]",
     "Annual close/intraday limit counts and next-day behavior."},
    {"history", ViewKind::history, "市场涨跌停温度历史", "[\"all\"]",
     "[\"list/func_zdtfx107_1.jsn\"]",
     "Daily market breadth, seal amounts, broken boards and streak distribution."},
    {"daily", ViewKind::daily, "指定日涨跌停证券",
     "[\"all\",\"limit-up\",\"limit-down\"]",
     "[\"zdtfx2/<YYYYMMDD>.jsn\",\"zdtfx3/<YYYYMMDD>.jsn\"]",
     "Date-keyed limit-up and limit-down member tables."},
    {"security", ViewKind::security, "单票涨跌停复盘", "[\"all\"]",
     "[\"zdtfx1/<market><code>.jsn\"]",
     "Current/annual matches plus per-security limit reason history."},
}};

template <typename Values, typename Projection>
constexpr bool unique_by(const Values& values, Projection projection) {
    for (std::size_t left = 0; left < values.size(); ++left)
        for (std::size_t right = left + 1; right < values.size(); ++right)
            if (projection(values[left]) == projection(values[right])) return false;
    return true;
}

static_assert(unique_by(resources,
    [](const ResourceSpec& value) { return std::string_view(value.resource); }));
static_assert(unique_by(resources,
    [](const ResourceSpec& value) { return value.normalize; }));
static_assert(unique_by(views,
    [](const ViewSpec& value) { return std::string_view(value.id); }));

}  // namespace

const std::array<ResourceSpec, 5>& fixed_resources() { return resources; }

const ResourceSpec& resource_spec(NormalizeKind kind) {
    const auto found = std::find_if(resources.begin(), resources.end(),
        [&](const ResourceSpec& value) { return value.normalize == kind; });
    if (found == resources.end()) throw Error("unknown fixed limit-review resource");
    return *found;
}

const ViewSpec& view_spec(std::string_view value) {
    const auto normalized = lower_ascii(trim(std::string(value)));
    const auto found = std::find_if(views.begin(), views.end(),
        [&](const ViewSpec& entry) { return entry.id == normalized; });
    if (found == views.end())
        throw Error("view must be catalog, current, annual, history, daily, or security");
    return *found;
}

Json catalog_document() {
    Json rows = Json::array();
    for (const auto& view : views) {
        if (view.kind == ViewKind::catalog) continue;
        Json row = Json::object();
        row["id"] = view.id;
        row["label"] = view.label;
        row["categories"] = Json::parse(view.categories_json);
        row["resources"] = Json::parse(view.resources_json);
        row["semantics"] = view.semantics;
        rows.push_back(std::move(row));
    }
    Json result = Json::object();
    result["schema"] = "tdx-market-limit-review-catalog-native-v1";
    result["view_count"] = static_cast<std::uint64_t>(rows.size());
    result["views"] = std::move(rows);
    result["boundary"] =
        "limit-review is TDX post-market/non-realtime JSN analysis; limit-quality remains live L1 depth, seal and auction quality.";
    return result;
}

}  // namespace tdx::detail::limit_review
