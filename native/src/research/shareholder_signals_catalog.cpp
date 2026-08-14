#include "shareholder_signals_internal.hpp"

#include <algorithm>

namespace tdx::detail::shareholder_signals {
namespace {

constexpr std::array<ResourceSpec, 5> resources{{
    {"list/func_cwnscg101_1.jsn", ResourceKind::notable_investors,
     "notable-investors", "牛散持股", normalize_notable_signal_row},
    {"list/func_jgxc101_1.jsn", ResourceKind::institution_accumulation,
     "institution-accumulation", "机构增持与股东收缩", normalize_institution_signal_row},
    {"list/func_jgzd101_1.jsn", ResourceKind::research_growth,
     "research-growth", "成长与调研活跃", normalize_research_signal_row},
    {"list/func_xszgp101_1.jsn", ResourceKind::small_cap_institution,
     "small-cap-institution", "小市值机构增持", normalize_small_cap_signal_row},
    {"list/func_nscg101_1.jsn", ResourceKind::investor_directory,
     "investor-directory", "知名自然人持股目录", nullptr},
}};

constexpr std::array<ViewSpec, 6> views{{
    {"all", ViewKind::all, nullptr, false},
    {"notable-investors", ViewKind::notable_investors, "notable-investors", false},
    {"institution-accumulation", ViewKind::institution_accumulation,
     "institution-accumulation", false},
    {"research-growth", ViewKind::research_growth, "research-growth", false},
    {"small-cap-institution", ViewKind::small_cap_institution,
     "small-cap-institution", false},
    {"investor-directory", ViewKind::investor_directory, nullptr, true},
}};

constexpr std::array<SortSpec, 8> sorts{{
    {"signal", SortKind::signal, "signal_value"},
    {"holding-value", SortKind::holding_value, "holding_value_yuan"},
    {"institution-growth", SortKind::institution_growth,
     "institution_holding_growth_pct"},
    {"holder-change", SortKind::holder_change, "shareholder_count_change_pct"},
    {"research-6m", SortKind::research_6m, "research_count_6m"},
    {"profit-growth", SortKind::profit_growth, "net_profit_growth_pct"},
    {"return-6m", SortKind::return_6m, "return_6m_pct"},
    {"code", SortKind::code, nullptr},
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
    [](const ResourceSpec& value) { return std::string_view(value.id); }));
static_assert(unique_by(resources,
    [](const ResourceSpec& value) { return value.kind; }));
static_assert(unique_by(views,
    [](const ViewSpec& value) { return std::string_view(value.id); }));
static_assert(unique_by(sorts,
    [](const SortSpec& value) { return std::string_view(value.id); }));

}  // namespace

const std::array<ResourceSpec, 5>& all_resources() { return resources; }

const ResourceSpec& resource_spec(std::string_view resource) {
    const auto found = std::find_if(resources.begin(), resources.end(),
        [&](const ResourceSpec& entry) { return entry.resource == resource; });
    if (found == resources.end())
        throw Error("unknown shareholder-signals resource: " + std::string(resource));
    return *found;
}

const ResourceSpec& resource_spec(ResourceKind kind) {
    const auto found = std::find_if(resources.begin(), resources.end(),
        [&](const ResourceSpec& entry) { return entry.kind == kind; });
    if (found == resources.end()) throw Error("unknown shareholder-signals resource kind");
    return *found;
}

const ViewSpec& view_spec(std::string_view view) {
    const auto normalized = lower_ascii(trim(std::string(view)));
    const auto found = std::find_if(views.begin(), views.end(),
        [&](const ViewSpec& entry) { return entry.id == normalized; });
    if (found == views.end()) throw Error("unsupported shareholder-signals view");
    return *found;
}

const SortSpec& sort_spec(std::string_view sort) {
    const auto normalized = lower_ascii(trim(std::string(sort)));
    const auto found = std::find_if(sorts.begin(), sorts.end(),
        [&](const SortSpec& entry) { return entry.id == normalized; });
    if (found == sorts.end()) throw Error("unsupported shareholder-signals sort");
    return *found;
}

}  // namespace tdx::detail::shareholder_signals
