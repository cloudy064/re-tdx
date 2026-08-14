#include "hk_events_internal.hpp"

#include <algorithm>

namespace tdx::detail {
namespace {

constexpr std::array<HkEventResourceDefinition, 4> kResources{{
    {"list/func_ggrl102_1.jsn", "dividend", "分红派息", "dividends"},
    {"list/func_ggrl103_1.jsn", "holding-disclosure", "权益披露", "holdings"},
    {"list/func_ggrl104_1.jsn", "short-selling", "沽空统计", "short-selling"},
    {"list/func_ggrl105_1.jsn", "listing-application", "上市申请", "applications"},
}};

constexpr bool unique_resource_definitions() {
    for (std::size_t left = 0; left < kResources.size(); ++left)
        for (std::size_t right = left + 1; right < kResources.size(); ++right)
            if (kResources[left].resource == kResources[right].resource ||
                kResources[left].kind == kResources[right].kind ||
                kResources[left].view == kResources[right].view)
                return false;
    return true;
}

static_assert(unique_resource_definitions(),
              "HK event resources, kinds, and views must be unique");

}  // namespace

const std::array<HkEventResourceDefinition, 4>& hk_event_resources() {
    return kResources;
}

const HkEventResourceDefinition& find_hk_event_resource(
    std::string_view resource) {
    const auto found = std::find_if(kResources.begin(), kResources.end(),
        [&](const auto& definition) {
            return definition.resource == resource;
        });
    if (found == kResources.end())
        throw Error("unknown HK event resource: " + std::string(resource));
    return *found;
}

const HkEventResourceDefinition* find_hk_event_view(std::string_view view) {
    const auto found = std::find_if(kResources.begin(), kResources.end(),
        [&](const auto& definition) {
            return definition.view == view;
        });
    return found == kResources.end() ? nullptr : &*found;
}

const std::vector<std::string>& hk_event_resource_paths() {
    static const std::vector<std::string> paths = [] {
        std::vector<std::string> result;
        result.reserve(kResources.size());
        for (const auto& definition : kResources)
            result.emplace_back(definition.resource);
        return result;
    }();
    return paths;
}

}  // namespace tdx::detail
