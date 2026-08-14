#include "capital_strength_internal.hpp"

#include <algorithm>

namespace tdx::detail::capital_strength {
namespace {

constexpr std::array<PeriodSpec, 5> periods{{
    {"5d", "5日", "list/func_qszj101_1.jsn", "zf5", "zjlr5", "zljlr5", 0},
    {"10d", "10日", "list/func_qszj102_1.jsn", "zf10", "zjlr10", "zljlr10", 1},
    {"20d", "20日", "list/func_qszj103_1.jsn", "zf20", "zjlr20", "zljlr20", 2},
    {"30d", "30日", "list/func_qszj104_1.jsn", "zf30", "zjlr30", "zljlr30", 3},
    {"3m", "近3月", "list/func_qszj105_1.jsn", "zf3y", "zjlr3y", "zljlr3y", 4},
}};

struct PeriodAlias { std::string_view alias; std::string_view id; };
constexpr std::array<PeriodAlias, 20> period_aliases{{
    {"5d", "5d"}, {"5", "5d"}, {"5day", "5d"}, {"5-day", "5d"},
    {"10d", "10d"}, {"10", "10d"}, {"10day", "10d"}, {"10-day", "10d"},
    {"20d", "20d"}, {"20", "20d"}, {"20day", "20d"}, {"20-day", "20d"},
    {"30d", "30d"}, {"30", "30d"}, {"30day", "30d"}, {"30-day", "30d"},
    {"3m", "3m"}, {"3month", "3m"}, {"3-month", "3m"}, {"quarter", "3m"},
}};

constexpr std::array<ViewSpec, 4> views{{
    {"ranking", ViewKind::ranking, "ddx", "desc", false, false},
    {"confluence", ViewKind::confluence, "period-count", "desc", true, false},
    {"security", ViewKind::security, "period", "asc", true, true},
    {"catalog", ViewKind::catalog, "ddx", "desc", false, false},
}};

constexpr unsigned mask(ViewKind view) {
    return 1U << static_cast<unsigned>(view);
}

constexpr unsigned ranking_security = mask(ViewKind::ranking) | mask(ViewKind::security);
constexpr std::array<SortSpec, 13> sorts{{
    {"ddx", SortKind::ddx, ranking_security, "ddx_float_share_pct"},
    {"return", SortKind::return_pct, ranking_security, "period_return_pct"},
    {"total-net", SortKind::total_net, ranking_security, "total_net_inflow_yuan"},
    {"main-net", SortKind::main_net, ranking_security, "main_net_inflow_yuan"},
    {"float-shares", SortKind::float_shares, ranking_security, "float_shares"},
    {"source-rank", SortKind::source_rank, ranking_security, "source_rank"},
    {"period", SortKind::period, ranking_security, "period"},
    {"code", SortKind::code, ranking_security | mask(ViewKind::confluence), nullptr},
    {"period-count", SortKind::period_count, mask(ViewKind::confluence), "period_count"},
    {"average-ddx", SortKind::average_ddx, mask(ViewKind::confluence),
     "average_ddx_float_share_pct"},
    {"maximum-ddx", SortKind::maximum_ddx, mask(ViewKind::confluence),
     "maximum_ddx_float_share_pct"},
    {"minimum-ddx", SortKind::minimum_ddx, mask(ViewKind::confluence),
     "minimum_ddx_float_share_pct"},
    {"best-rank", SortKind::best_rank, mask(ViewKind::confluence), "best_source_rank"},
}};

template <typename Values, typename Projection>
constexpr bool unique_by(const Values& values, Projection projection) {
    for (std::size_t left = 0; left < values.size(); ++left)
        for (std::size_t right = left + 1; right < values.size(); ++right)
            if (projection(values[left]) == projection(values[right])) return false;
    return true;
}

constexpr bool valid_period_alias_targets() {
    for (const auto& alias : period_aliases) {
        bool found = false;
        for (const auto& period : periods)
            if (alias.id == period.id) found = true;
        if (!found) return false;
    }
    return true;
}

constexpr bool valid_view_defaults() {
    for (const auto& view : views) {
        bool found = false;
        for (const auto& sort : sorts)
            if (std::string_view(view.default_sort) == sort.id) found = true;
        if (!found) return false;
    }
    return true;
}

static_assert(unique_by(periods,
    [](const PeriodSpec& value) { return std::string_view(value.id); }));
static_assert(unique_by(periods,
    [](const PeriodSpec& value) { return std::string_view(value.resource); }));
static_assert(unique_by(period_aliases,
    [](const PeriodAlias& value) { return value.alias; }));
static_assert(unique_by(views,
    [](const ViewSpec& value) { return std::string_view(value.id); }));
static_assert(unique_by(sorts,
    [](const SortSpec& value) { return std::string_view(value.id); }));
static_assert(valid_period_alias_targets());
static_assert(valid_view_defaults());

}  // namespace

const std::array<PeriodSpec, 5>& all_period_specs() { return periods; }

const PeriodSpec& period_spec(std::string_view value) {
    const auto normalized = lower_ascii(trim(std::string(value)));
    const auto alias = std::find_if(period_aliases.begin(), period_aliases.end(),
        [&](const PeriodAlias& entry) { return entry.alias == normalized; });
    if (alias == period_aliases.end()) {
        throw Error("period must be 5d, 10d, 20d, 30d, or 3m");
    }
    const auto found = std::find_if(periods.begin(), periods.end(),
        [&](const PeriodSpec& entry) { return entry.id == alias->id; });
    return *found;
}

const ViewSpec& view_spec(std::string_view value) {
    const auto normalized = lower_ascii(trim(std::string(value)));
    const auto found = std::find_if(views.begin(), views.end(),
        [&](const ViewSpec& entry) { return entry.id == normalized; });
    if (found == views.end())
        throw Error("view must be ranking, confluence, security, or catalog");
    return *found;
}

const SortSpec& sort_spec(ViewKind view, std::string_view value) {
    const auto normalized = lower_ascii(trim(std::string(value)));
    const auto found = std::find_if(sorts.begin(), sorts.end(),
        [&](const SortSpec& entry) { return entry.id == normalized; });
    if (found != sorts.end() && (found->allowed_views & mask(view))) return *found;
    if (view == ViewKind::confluence)
        throw Error("confluence sort must be period-count, average-ddx, maximum-ddx, minimum-ddx, best-rank, or code");
    throw Error("ranking sort must be ddx, return, total-net, main-net, float-shares, source-rank, period, or code");
}

int period_order(std::string_view value) { return period_spec(value).order; }

Json catalog_rows() {
    Json result = Json::array();
    for (const auto& spec : periods) {
        Json row = Json::object();
        row["period"] = spec.id;
        row["label"] = spec.label;
        row["resource"] = spec.resource;
        row["source_limit"] = 100;
        row["source_sort"] = "ddx-desc";
        row["return_field"] = spec.return_key;
        row["total_net_inflow_field"] = spec.total_net_key;
        row["main_net_inflow_field"] = spec.main_net_key;
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace tdx::detail::capital_strength
