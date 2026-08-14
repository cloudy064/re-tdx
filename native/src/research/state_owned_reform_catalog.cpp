#include "state_owned_reform_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>

namespace tdx {

const std::vector<StateOwnedDimension>& state_owned_dimensions() {
    static const std::vector<StateOwnedDimension> dimensions{
        {"industry", "按行业", "list/func_gqgg101_1.jsn", "25701", "HYFL"},
        {"region", "按地区", "list/func_gqgg102_1.jsn", "25702", "DQFL"},
        {"integration", "整合预期", "list/func_gqgg103_1.jsn", "25703", "HYFL"},
        {"company", "公司系", "list/func_gqgg104_1.jsn", "25704", "HYFL"},
    };
    return dimensions;
}

namespace state_owned_detail {
namespace {

const std::string restructuring{"list/func_gqgg106_1.jsn"};

constexpr std::array<ViewDefinition, 5> views{{
    {"groups", ViewKind::groups},
    {"group", ViewKind::group},
    {"security", ViewKind::security},
    {"restructuring", ViewKind::restructuring},
    {"catalog", ViewKind::catalog},
}};

constexpr std::array<std::pair<std::string_view, GroupSortKind>, 3> group_sorts{{
    {"count", GroupSortKind::count},
    {"name", GroupSortKind::name},
    {"id", GroupSortKind::id},
}};

constexpr std::array<std::pair<std::string_view, ReformSortKind>, 4> reform_sorts{{
    {"date", ReformSortKind::date},
    {"profit", ReformSortKind::profit},
    {"control", ReformSortKind::control},
    {"code", ReformSortKind::code},
}};

}  // namespace

const std::array<ViewDefinition, 5>& view_definitions() {
    return views;
}

const ViewDefinition* find_view(std::string_view name) {
    const auto found = std::find_if(views.begin(), views.end(),
        [name](const ViewDefinition& item) { return item.name == name; });
    return found == views.end() ? nullptr : &*found;
}

const std::string& restructuring_resource() {
    return restructuring;
}

const StateOwnedDimension& find_dimension(const std::string& value) {
    const auto wanted = lower_ascii(trim(value));
    const auto& dimensions = state_owned_dimensions();
    const auto found = std::find_if(dimensions.begin(), dimensions.end(),
        [&](const StateOwnedDimension& item) { return item.name == wanted; });
    if (found == dimensions.end())
        throw Error("dimension must be industry, region, integration, or company");
    return *found;
}

GroupSortKind group_sort_kind(std::string_view name) {
    const auto found = std::find_if(group_sorts.begin(), group_sorts.end(),
        [name](const auto& item) { return item.first == name; });
    if (found == group_sorts.end())
        throw Error("group sort must be count, name, or id");
    return found->second;
}

ReformSortKind reform_sort_kind(std::string_view name) {
    const auto found = std::find_if(reform_sorts.begin(), reform_sorts.end(),
        [name](const auto& item) { return item.first == name; });
    if (found == reform_sorts.end())
        throw Error("restructuring sort must be date, profit, control, or code");
    return found->second;
}

bool descending_order(std::string_view name) {
    if (name == "desc") return true;
    if (name == "asc") return false;
    throw Error("order must be asc or desc");
}

}  // namespace state_owned_detail
}  // namespace tdx
