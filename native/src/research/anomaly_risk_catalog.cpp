#include "anomaly_risk_internal.hpp"

#include <algorithm>
#include <array>

namespace tdx::detail::anomaly_risk {
namespace {

constexpr std::array<ViewSpec, 2> views{{
    {"statistics", ViewKind::statistics, "异常波动统计", "2044",
     "sc_ydjj.xml", "mod_abnormal_gold64.dll", true, 0, 20, 100, true,
     normalize_anomaly_statistics_rows},
    {"suspension-risk", ViewKind::suspension_risk, "异动停牌风险", "200720",
     "sc_gjyd.xml", "mod_tcihq.dll", false, -1, 0, 100, false,
     normalize_suspension_risk_rows},
}};

struct ViewAlias { std::string_view alias; ViewKind kind; };
constexpr std::array<ViewAlias, 6> view_aliases{{
    {"statistics", ViewKind::statistics}, {"stats", ViewKind::statistics},
    {"relative", ViewKind::statistics},
    {"suspension-risk", ViewKind::suspension_risk},
    {"suspension", ViewKind::suspension_risk},
    {"risk", ViewKind::suspension_risk},
}};

constexpr std::array<WarningSpec, 4> warnings{{
    {0, "none", "无预警", false},
    {1, "repeat-suspension", "可能因为异动再次停牌", true},
    {2, "possible-suspension", "可能因为异动停牌", true},
    {3, "triggered", "触发异动", true},
}};

struct WarningAlias {
    std::string_view alias;
    int code;
};
constexpr std::array<WarningAlias, 8> warning_aliases{{
    {"none", 0}, {"0", 0},
    {"repeat-suspension", 1}, {"1", 1},
    {"possible-suspension", 2}, {"2", 2},
    {"triggered", 3}, {"3", 3},
}};

template <typename Values, typename Projection>
constexpr bool unique_by(const Values& values, Projection projection) {
    for (std::size_t left = 0; left < values.size(); ++left)
        for (std::size_t right = left + 1; right < values.size(); ++right)
            if (projection(values[left]) == projection(values[right])) return false;
    return true;
}

static_assert(unique_by(views,
    [](const ViewSpec& value) { return std::string_view(value.id); }));
static_assert(unique_by(views,
    [](const ViewSpec& value) { return std::string_view(value.request_id); }));
static_assert(unique_by(view_aliases,
    [](const ViewAlias& value) { return value.alias; }));
static_assert(unique_by(warnings,
    [](const WarningSpec& value) { return value.code; }));
static_assert(unique_by(warnings,
    [](const WarningSpec& value) { return std::string_view(value.status); }));
static_assert(unique_by(warning_aliases,
    [](const WarningAlias& value) { return value.alias; }));

}  // namespace

const ViewSpec& view_spec(std::string_view raw) {
    const auto value = lower_ascii(trim(std::string(raw)));
    const auto alias = std::find_if(view_aliases.begin(), view_aliases.end(),
        [&](const ViewAlias& entry) { return entry.alias == value; });
    if (alias == view_aliases.end())
        throw Error("view must be statistics or suspension-risk");
    return *std::find_if(views.begin(), views.end(),
        [&](const ViewSpec& entry) { return entry.kind == alias->kind; });
}

const WarningSpec& warning_spec(int code) {
    const auto found = std::find_if(warnings.begin(), warnings.end(),
        [&](const WarningSpec& entry) { return entry.code == code; });
    if (found == warnings.end())
        throw Error("suspension-risk row has unknown warning code: " +
                    std::to_string(code));
    return *found;
}

const WarningSpec* warning_filter_spec(std::string_view raw) {
    const auto value = lower_ascii(trim(std::string(raw)));
    if (value.empty() || value == "all") return nullptr;
    const auto alias = std::find_if(warning_aliases.begin(), warning_aliases.end(),
        [&](const WarningAlias& entry) { return entry.alias == value; });
    if (alias == warning_aliases.end())
        throw Error(
            "warning must be all, none, repeat-suspension, possible-suspension, or triggered");
    return &warning_spec(alias->code);
}

Json available_views() {
    Json result = Json::array();
    for (const auto& view : views) result.push_back(view.id);
    return result;
}

Json warning_catalog() {
    Json result = Json::array();
    for (const auto& warning : warnings) {
        Json item = Json::object();
        item["code"] = warning.code;
        item["status"] = warning.status;
        item["label"] = warning.label;
        item["active"] = warning.active;
        result.push_back(std::move(item));
    }
    return result;
}

}  // namespace tdx::detail::anomaly_risk
