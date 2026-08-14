#include "special_situations_internal.hpp"

#include <algorithm>

namespace tdx::special_situations_detail {
namespace {

constexpr std::array<ResourceDefinition, 10> resources{{
    {ResourceKind::merger, "list/func_agtl101_1.jsn", "merger", "吸收合并"},
    {ResourceKind::b_to_h, "list/func_agtl102_1.jsn", "b-to-h", "B转H"},
    {ResourceKind::market_cap_risk, "list/func_cdgc101_1.jsn", "market-cap-risk", "市值管理预警"},
    {ResourceKind::major_restructuring_plan, "list/func_qxfa105_1.jsn", "major-restructuring-plan", "重大重组预案"},
    {ResourceKind::major_restructuring_review, "list/func_qxfa106_1.jsn", "major-restructuring-review", "重大重组审核"},
    {ResourceKind::major_restructuring_completed, "list/func_qxfa110_1.jsn", "major-restructuring-completed", "重大重组实施"},
    {ResourceKind::ordinary_merger_plan, "list/func_qxfa111_1.jsn", "ordinary-merger-plan", "普通并购预案"},
    {ResourceKind::neeq_transfer_plan, "list/func_xsbtj101_1.jsn", "neeq-transfer-plan", "三板拟转A股"},
    {ResourceKind::neeq_regulation, "list/func_xsbtj102_1.jsn", "neeq-regulation", "三板自律监管"},
    {ResourceKind::neeq_transfer_completed, "list/func_yzb101_1.jsn", "neeq-transfer-completed", "已转板"},
}};

constexpr bool unique_resources() {
    for (std::size_t left = 0; left < resources.size(); ++left)
        for (std::size_t right = left + 1; right < resources.size(); ++right)
            if (resources[left].resource == resources[right].resource) return false;
    return true;
}

static_assert(unique_resources());

}  // namespace

const std::array<ResourceDefinition, 10>& resource_definitions() {
    return resources;
}

const std::vector<std::string>& resource_names() {
    static const auto names = [] {
        std::vector<std::string> result;
        result.reserve(resources.size());
        for (const auto& resource : resources)
            result.emplace_back(resource.resource);
        return result;
    }();
    return names;
}

const ResourceDefinition* find_resource_definition(std::string_view resource) {
    const auto found = std::find_if(resources.begin(), resources.end(),
        [resource](const ResourceDefinition& candidate) {
            return candidate.resource == resource;
        });
    return found == resources.end() ? nullptr : &*found;
}

}  // namespace tdx::special_situations_detail
