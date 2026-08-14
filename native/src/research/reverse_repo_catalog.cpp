#include "reverse_repo_internal.hpp"

#include <algorithm>
#include <array>

namespace tdx::detail::reverse_repo {
namespace {
constexpr const char* schedule = "list/func_gznhg100_1.jsn";
constexpr std::array<ViewSpec, 3> views{{
    {"rates", ViewKind::rates, false},
    {"security", ViewKind::security, true},
    {"catalog", ViewKind::catalog, false},
}};
constexpr std::array<SortSpec, 11> sorts{{
    {"rate", SortKind::rate, "annualized_rate_pct", false},
    {"net-rate", SortKind::net_rate, "net_annualized_rate_pct", false},
    {"gross-interest", SortKind::gross_interest, "gross_interest_yuan", false},
    {"net-interest", SortKind::net_interest, "net_interest_yuan", false},
    {"term", SortKind::term, "term_days", false},
    {"interest-days", SortKind::interest_days, "interest_days", false},
    {"available-date", SortKind::available_date, "funds_available_date", true},
    {"withdrawable-date", SortKind::withdrawable_date, "funds_withdrawable_date", true},
    {"turnover", SortKind::turnover, "turnover_amount_yuan", false},
    {"code", SortKind::code, "repo_id", true},
    {"source-rank", SortKind::source_rank, "source_rank", false},
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
static_assert(unique_by(sorts,
    [](const SortSpec& value) { return std::string_view(value.id); }));
}  // namespace

const char* schedule_resource() { return schedule; }
const ViewSpec& view_spec(std::string_view raw) {
    const auto value = lower_ascii(trim(std::string(raw)));
    const auto found = std::find_if(views.begin(), views.end(),
        [&](const ViewSpec& entry) { return value == entry.id; });
    if (found == views.end()) throw Error("view must be rates, security, or catalog");
    return *found;
}
const SortSpec& sort_spec(std::string_view raw) {
    const auto value = lower_ascii(trim(std::string(raw)));
    const auto found = std::find_if(sorts.begin(), sorts.end(),
        [&](const SortSpec& entry) { return value == entry.id; });
    if (found == sorts.end())
        throw Error("sort must be rate, net-rate, gross-interest, net-interest, term, interest-days, available-date, withdrawable-date, turnover, code, or source-rank");
    return *found;
}
Json catalog_rows() {
    Json result = Json::array();
    Json rates = Json::object();
    rates["view"] = "rates"; rates["label"] = "沪深国债逆回购收益比较";
    rates["resource"] = schedule; rates["quote_command"] = "0x054C";
    rates["fields"] = "term/interest-days,fee,settlement/available/withdrawable dates,annualized-rate,gross/net interest";
    result.push_back(std::move(rates));
    Json security = Json::object();
    security["view"] = "security"; security["label"] = "单一逆回购品种";
    security["resource"] = schedule;
    security["fields"] = "one repo schedule and optional public L1 quote";
    result.push_back(std::move(security));
    Json aliases = Json::object();
    aliases["view"] = "aliases"; aliases["label"] = "客户端等价资源";
    aliases["resources"] = Json::parse(
        "[\"list/func_gznhg100_1.jsn\",\"list/func_gznhg101_1.jsn\",\"list/func_gznhg102_1.jsn\",\"list/gxjty_zq_gznhg101_1.jsn\"]");
    aliases["semantics"] = "combined current page, legacy Shanghai/Shenzhen split pages, and alternate bond page expose the same settlement calendar";
    result.push_back(std::move(aliases));
    return result;
}
}  // namespace tdx::detail::reverse_repo
