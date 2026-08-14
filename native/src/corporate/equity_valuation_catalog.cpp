#include "equity_valuation_internal.hpp"

#include <algorithm>
#include <array>

namespace tdx::detail::equity_valuation {
namespace {

constexpr std::array<ViewSpec, 5> view_values{{
    {"pe-industries", ViewKind::pe_industries, "200302", "gp_gz_peg.xml",
     true, false, false, SortKind::industry, normalize_pe_industry_row},
    {"pe-security-history", ViewKind::pe_security_history, "200300", "gp_gz_peg.xml",
     true, true, false, SortKind::history, normalize_pe_history_row},
    {"pe-industry-members", ViewKind::pe_industry_members, "200301", "gp_gz_peg.xml",
     true, false, true, SortKind::security, normalize_pe_member_row},
    {"pb-roe-industries", ViewKind::pb_roe_industries, "200305", "gp_gz_PB-ROE.xml",
     false, false, false, SortKind::industry, normalize_pb_industry_row},
    {"pb-roe-members", ViewKind::pb_roe_members, "200303", "gp_gz_PB-ROE.xml",
     false, false, true, SortKind::security, normalize_pb_member_row},
}};

struct ViewAlias {
    std::string_view alias;
    ViewKind kind;
};

constexpr std::array<ViewAlias, 10> aliases{{
    {"pe-industries", ViewKind::pe_industries},
    {"pe-industry", ViewKind::pe_industries},
    {"pe-security-history", ViewKind::pe_security_history},
    {"pe-history", ViewKind::pe_security_history},
    {"pe-industry-members", ViewKind::pe_industry_members},
    {"pe-members", ViewKind::pe_industry_members},
    {"pb-roe-industries", ViewKind::pb_roe_industries},
    {"pb-industries", ViewKind::pb_roe_industries},
    {"pb-roe-members", ViewKind::pb_roe_members},
    {"pb-members", ViewKind::pb_roe_members},
}};

template <typename Values, typename Projection>
constexpr bool unique_by(const Values& values, Projection projection) {
    for (std::size_t left = 0; left < values.size(); ++left)
        for (std::size_t right = left + 1; right < values.size(); ++right)
            if (projection(values[left]) == projection(values[right])) return false;
    return true;
}

static_assert(unique_by(view_values,
    [](const ViewSpec& value) { return std::string_view(value.id); }));
static_assert(unique_by(view_values,
    [](const ViewSpec& value) { return std::string_view(value.request_id); }));
static_assert(unique_by(aliases,
    [](const ViewAlias& value) { return value.alias; }));

}  // namespace

const ViewSpec& view_spec(std::string_view raw) {
    const auto normalized = lower_ascii(trim(std::string(raw)));
    const auto alias = std::find_if(aliases.begin(), aliases.end(),
        [&](const ViewAlias& value) { return value.alias == normalized; });
    if (alias == aliases.end())
        throw Error("view must be pe-industries, pe-security-history, "
                    "pe-industry-members, pb-roe-industries, or pb-roe-members");
    const auto view = std::find_if(view_values.begin(), view_values.end(),
        [&](const ViewSpec& value) { return value.kind == alias->kind; });
    return *view;
}

}  // namespace tdx::detail::equity_valuation
