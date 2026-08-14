#include "thematic_opportunities_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>

namespace tdx::detail::thematic_opportunities {
namespace {

constexpr std::array<ResourceDefinition, 5> kResources{{
    {ResourceRole::industry_groups, "list/func_ydyl101_1.jsn", "industry",
     "细分行业", "ZLHY", "DLHY", "ydyl1/", false},
    {ResourceRole::region_groups, "list/func_ydyl102_1.jsn", "region",
     "核心区域", "HXDQ", "DLQY", "ydyl1/", false},
    {ResourceRole::completed_hype, "list/func_rdhs101_1.jsn", {}, {}, {}, {}, {}, false},
    {ResourceRole::active_hype, "list/func_rdhs102_1.jsn", {}, {}, {}, {}, {}, false},
    {ResourceRole::legacy_themes, "list/func_xnxs101_1.jsn", "legacy-client-theme",
     "遗留客户端主题", "flzl", "flname", "xnxs/", true},
}};

constexpr std::array<std::string_view, 6> kViews{{
    "catalog", "groups", "group", "security", "hype-completed", "hype-active",
}};
constexpr std::array<std::string_view, 4> kTypes{{
    "all", "industry", "region", "legacy-client-theme",
}};
constexpr std::array<std::string_view, 4> kGroupSorts{{
    "name", "members", "id", "type",
}};
constexpr std::array<std::string_view, 2> kOrders{{"asc", "desc"}};
constexpr std::string_view kLegacyDeclaredPageName = "虚拟现实";

constexpr bool unique_catalog() {
    for (std::size_t left = 0; left < kResources.size(); ++left) {
        for (std::size_t right = left + 1; right < kResources.size(); ++right) {
            if (kResources[left].role == kResources[right].role ||
                kResources[left].resource == kResources[right].resource)
                return false;
            if (!kResources[left].group_type.empty() &&
                kResources[left].group_type == kResources[right].group_type)
                return false;
        }
    }
    return true;
}

template <std::size_t Size>
bool contains(const std::array<std::string_view, Size>& values,
              std::string_view value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

static_assert(unique_catalog(), "thematic opportunity resource catalog must be unique");

}  // namespace

const std::array<ResourceDefinition, 5>& resource_definitions() {
    return kResources;
}

const ResourceDefinition& resource_for_role(ResourceRole role) {
    const auto found = std::find_if(kResources.begin(), kResources.end(),
        [role](const auto& definition) { return definition.role == role; });
    if (found == kResources.end()) throw Error("unknown thematic opportunity resource role");
    return *found;
}

const ResourceDefinition& resource_for_group_type(std::string_view type) {
    const auto found = std::find_if(kResources.begin(), kResources.end(),
        [type](const auto& definition) { return definition.group_type == type; });
    if (found == kResources.end())
        throw Error("opportunity-group type must be industry, region, or legacy-client-theme");
    return *found;
}

const std::vector<std::string>& resource_paths() {
    static const auto paths = [] {
        std::vector<std::string> result;
        result.reserve(kResources.size());
        for (const auto& definition : kResources)
            result.emplace_back(definition.resource);
        return result;
    }();
    return paths;
}

std::string_view legacy_declared_page_name() { return kLegacyDeclaredPageName; }
bool valid_view(std::string_view value) { return contains(kViews, value); }
bool valid_type(std::string_view value) { return contains(kTypes, value); }
bool valid_group_sort(std::string_view value) { return contains(kGroupSorts, value); }
bool valid_order(std::string_view value) { return contains(kOrders, value); }

}  // namespace tdx::detail::thematic_opportunities
