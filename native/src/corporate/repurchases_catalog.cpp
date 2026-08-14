#include "repurchases_internal.hpp"

#include <algorithm>

namespace tdx::detail::repurchases {
namespace {

constexpr std::array<CoreResourceSpec, 3> core_resource_values{{
    {CoreResourceRole::plans, "plans", "list/func_qxfa104_1.jsn"},
    {CoreResourceRole::monthly, "monthly", "list/func_hgtj101_1.jsn"},
    {CoreResourceRole::hong_kong, "hong_kong", "list/func_gghg101_1.jsn"},
}};

constexpr std::array<AnnualSpec, 3> annual_values{{
    {"all", "全市场", "list/func_hgrztj101_1.jsn", "hgrztj21701"},
    {"a", "A股", "list/func_hgrztj102_1.jsn", "hgrztj21702"},
    {"hk", "港股", "list/func_hgrztj103_1.jsn", "hgrztj21703"},
}};

constexpr std::array<ViewSpec, 4> view_values{{
    {"plans", ViewKind::plans},
    {"monthly", ViewKind::monthly},
    {"annual", ViewKind::annual},
    {"hong-kong", ViewKind::hong_kong},
}};

template <typename Values, typename Projection>
constexpr bool unique_by(const Values& values, Projection projection) {
    for (std::size_t left = 0; left < values.size(); ++left)
        for (std::size_t right = left + 1; right < values.size(); ++right)
            if (projection(values[left]) == projection(values[right])) return false;
    return true;
}

static_assert(unique_by(core_resource_values,
    [](const CoreResourceSpec& value) { return value.role; }));
static_assert(unique_by(core_resource_values,
    [](const CoreResourceSpec& value) { return std::string_view(value.resource); }));
static_assert(unique_by(annual_values,
    [](const AnnualSpec& value) { return std::string_view(value.id); }));
static_assert(unique_by(annual_values,
    [](const AnnualSpec& value) { return std::string_view(value.resource); }));
static_assert(unique_by(annual_values,
    [](const AnnualSpec& value) { return std::string_view(value.detail_namespace); }));
static_assert(unique_by(view_values,
    [](const ViewSpec& value) { return std::string_view(value.id); }));

}  // namespace

const std::array<CoreResourceSpec, 3>& core_resources() {
    return core_resource_values;
}

const CoreResourceSpec& core_resource(CoreResourceRole role) {
    const auto found = std::find_if(core_resource_values.begin(), core_resource_values.end(),
        [&](const CoreResourceSpec& value) { return value.role == role; });
    if (found == core_resource_values.end()) throw Error("unknown repurchase core resource");
    return *found;
}

const std::array<AnnualSpec, 3>& annual_specs() {
    return annual_values;
}

const AnnualSpec& annual_spec(const std::string& id) {
    const auto found = std::find_if(annual_values.begin(), annual_values.end(),
        [&](const AnnualSpec& value) { return value.id == id; });
    if (found == annual_values.end()) throw Error("segment must be all, a, or hk");
    return *found;
}

const ViewSpec& view_spec(const std::string& id) {
    const auto found = std::find_if(view_values.begin(), view_values.end(),
        [&](const ViewSpec& value) { return value.id == id; });
    if (found == view_values.end())
        throw Error("view must be plans, monthly, annual, or hong-kong");
    return *found;
}

}  // namespace tdx::detail::repurchases
