#include "threshold_stocks_internal.hpp"

#include <algorithm>
#include <array>

namespace tdx::detail::threshold_stocks {
namespace {

constexpr std::array<UniverseSpec, 2> universes{{
    {"high-price", UniverseKind::high_price, "百元股",
     "list/func_bygtj102_1.jsn", "zjs"},
    {"mega-cap", UniverseKind::mega_cap, "千亿市值",
     "list/func_qyjlb102_1.jsn", "dbsl"},
}};
struct UniverseAlias { std::string_view alias; UniverseKind kind; };
constexpr std::array<UniverseAlias, 8> universe_aliases{{
    {"high-price", UniverseKind::high_price},
    {"hundred-price", UniverseKind::high_price},
    {"hundred-yuan", UniverseKind::high_price},
    {"price", UniverseKind::high_price},
    {"mega-cap", UniverseKind::mega_cap},
    {"thousand-billion", UniverseKind::mega_cap},
    {"market-cap", UniverseKind::mega_cap},
    {"cap", UniverseKind::mega_cap},
}};
constexpr std::array<ViewSpec, 4> views{{
    {"history", ViewKind::history, SortDomain::history, false, false},
    {"members", ViewKind::members, SortDomain::members, false, true},
    {"security", ViewKind::security, SortDomain::members, true, true},
    {"catalog", ViewKind::catalog, SortDomain::none, false, false},
}};
constexpr std::array<SortSpec, 11> sorts{{
    {"date", SortDomain::history, SortKind::date},
    {"count", SortDomain::history, SortKind::count},
    {"market-cap", SortDomain::history, SortKind::market_cap},
    {"market-share", SortDomain::history, SortKind::market_share},
    {"entered", SortDomain::history, SortKind::entered},
    {"exited", SortDomain::history, SortKind::exited},
    {"code", SortDomain::members, SortKind::code},
    {"day-return", SortDomain::members, SortKind::day_return},
    {"threshold-value", SortDomain::members, SortKind::threshold_value},
    {"net-increase", SortDomain::members, SortKind::net_increase},
    {"status", SortDomain::members, SortKind::status},
}};

template <typename Values, typename Projection>
constexpr bool unique_by(const Values& values, Projection projection) {
    for (std::size_t left = 0; left < values.size(); ++left)
        for (std::size_t right = left + 1; right < values.size(); ++right)
            if (projection(values[left]) == projection(values[right])) return false;
    return true;
}

static_assert(unique_by(universes,
    [](const UniverseSpec& value) { return std::string_view(value.id); }));
static_assert(unique_by(universes,
    [](const UniverseSpec& value) { return std::string_view(value.master_resource); }));
static_assert(unique_by(universe_aliases,
    [](const UniverseAlias& value) { return value.alias; }));
static_assert(unique_by(views,
    [](const ViewSpec& value) { return std::string_view(value.id); }));
static_assert(unique_by(sorts,
    [](const SortSpec& value) { return std::string_view(value.id); }));

}  // namespace

const UniverseSpec& universe_spec(std::string_view raw) {
    const auto value = lower_ascii(trim(std::string(raw)));
    const auto alias = std::find_if(universe_aliases.begin(), universe_aliases.end(),
        [&](const UniverseAlias& entry) { return entry.alias == value; });
    if (alias == universe_aliases.end())
        throw Error("universe must be high-price or mega-cap");
    return *std::find_if(universes.begin(), universes.end(),
        [&](const UniverseSpec& entry) { return entry.kind == alias->kind; });
}

const std::array<UniverseSpec, 2>& all_universes() { return universes; }

const ViewSpec& view_spec(std::string_view raw) {
    const auto value = lower_ascii(trim(std::string(raw)));
    const auto found = std::find_if(views.begin(), views.end(),
        [&](const ViewSpec& entry) { return value == entry.id; });
    if (found == views.end())
        throw Error("view must be history, members, security, or catalog");
    return *found;
}

const SortSpec& sort_spec(SortDomain domain, std::string_view raw) {
    const auto value = lower_ascii(trim(std::string(raw)));
    const auto found = std::find_if(sorts.begin(), sorts.end(),
        [&](const SortSpec& entry) {
            return entry.domain == domain && value == entry.id;
        });
    if (found != sorts.end()) return *found;
    if (domain == SortDomain::history)
        throw Error("history sort must be date, count, market-cap, market-share, entered, or exited");
    throw Error("member sort must be code, day-return, threshold-value, net-increase, or status");
}

}  // namespace tdx::detail::threshold_stocks
