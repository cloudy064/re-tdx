#include "exchange_funds_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>

namespace tdx::exchange_fund_detail {

const KindDefinition* find_kind(std::string_view kind) {
    const auto found = std::find_if(
        kind_catalog.begin(), kind_catalog.end(),
        [kind](const KindDefinition& item) { return item.kind == kind; });
    return found == kind_catalog.end() ? nullptr : &*found;
}

const KindDefinition* find_view(std::string_view view) {
    const auto found = std::find_if(
        kind_catalog.begin(), kind_catalog.end(),
        [view](const KindDefinition& item) { return item.view == view; });
    return found == kind_catalog.end() ? nullptr : &*found;
}

const std::vector<std::string>& resource_names() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> result;
        result.reserve(resource_catalog.size());
        for (const auto& item : resource_catalog)
            result.emplace_back(item.resource);
        return result;
    }();
    return names;
}

std::string resource_kind(std::string_view resource) {
    const auto found = std::find_if(
        resource_catalog.begin(), resource_catalog.end(),
        [resource](const ResourceDefinition& item) {
            return item.resource == resource;
        });
    if (found == resource_catalog.end())
        throw Error("unknown exchange-fund resource: " + std::string(resource));
    return std::string(found->kind);
}

std::string kind_label(std::string_view kind) {
    const auto* definition = find_kind(kind);
    if (!definition)
        throw Error("unknown exchange-fund kind: " + std::string(kind));
    return std::string(definition->label);
}

std::string view_kind(std::string_view view) {
    if (view == "all") return {};
    const auto* definition = find_view(view);
    return definition ? std::string(definition->kind) : std::string{};
}

bool valid_view(std::string_view view) {
    return view == "all" || find_view(view) != nullptr;
}

int kind_rank(std::string_view kind) {
    const auto* definition = find_kind(kind);
    return definition ? definition->rank : 10;
}

}  // namespace tdx::exchange_fund_detail
