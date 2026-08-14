#include "forecasts_internal.hpp"

#include <algorithm>

namespace tdx::forecast_detail {
namespace {

constexpr std::array<ResourceDefinition, 3> resources{{
    {ResourceKind::industry, "list/func_yjygtj101_1.jsn"},
    {ResourceKind::hong_kong, "list/func_ggyjyg101_1.jsn"},
    {ResourceKind::latest, "list/func_cbpl101_1.jsn"},
}};

constexpr std::array<ViewDefinition, 4> views{{
    {"industries", ViewKind::industries},
    {"securities", ViewKind::securities},
    {"latest", ViewKind::latest},
    {"hong-kong", ViewKind::hong_kong},
}};

constexpr std::array<std::string_view, 4> categories{{
    "all", "positive", "negative", "uncertain",
}};

}  // namespace

const std::array<ResourceDefinition, 3>& resource_definitions() {
    return resources;
}

const char* resource_name(ResourceKind kind) {
    const auto found = std::find_if(resources.begin(), resources.end(),
        [kind](const ResourceDefinition& item) { return item.kind == kind; });
    return found == resources.end() ? "" : found->path;
}

const std::array<ViewDefinition, 4>& view_definitions() {
    return views;
}

const ViewDefinition* find_view(std::string_view name) {
    const auto found = std::find_if(views.begin(), views.end(),
        [name](const ViewDefinition& item) { return item.name == name; });
    return found == views.end() ? nullptr : &*found;
}

bool valid_category(std::string_view category) {
    return std::find(categories.begin(), categories.end(), category) !=
        categories.end();
}

}  // namespace tdx::forecast_detail
