#include "strategic_themes_internal.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>

namespace tdx {

StrategicThemeService::FetchResult StrategicThemeService::fetch_detail(
    const std::string& theme_id, const StrategicThemeQuery& options) {
    auto source_theme_id = theme_id;
    const auto separator = source_theme_id.find(':');
    if (separator != std::string::npos)
        source_theme_id = source_theme_id.substr(separator + 1);
    const auto resource = "zttzty/" + source_theme_id + ".jsn";
    const auto now = std::time(nullptr);
    const auto found = detail_cache_.find(resource);
    if (!options.refresh && found != detail_cache_.end()) {
        const int age = static_cast<int>(
            std::max<std::time_t>(0, now - found->second.fetched_at));
        if (age < options.detail_cache_ttl_seconds)
            return {found->second.document, false, age};
    }
    auto document = fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
    detail_cache_[resource] = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

}  // namespace tdx
