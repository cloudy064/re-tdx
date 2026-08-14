#include "commodity_links_internal.hpp"

namespace tdx::commodity_links_detail {
namespace {

constexpr std::array<ViewDefinition, 7> views{{
    {"commodities", "商品行情", "list/func_zjtc103_1.jsn", "none",
     "quote-date", "quote-date|name|price|day-change|5d|10d|30d|60d|source-rank"},
    {"commodity", "商品关联股票与行业/ETF",
     "zjtc4/<commodity-id>, zjtc5/<commodity-id>", "commodity_id",
     "source-rank", "source-rank"},
    {"themes", "涨价题材", "list/func_zjtc101_1.jsn", "none",
     "latest-driver-date", "latest-driver-date|trigger-date|name|stocks|source-rank"},
    {"theme", "题材长期关联股与历史驱动",
     "zjtc1/<theme-id>, zjtc2/<theme-id>", "theme_id",
     "source-rank", "source-rank"},
    {"driver", "单次驱动事件关联股", "zjtc3/<driver-id>",
     "theme_id + driver_id", "source-rank", "source-rank"},
    {"security", "单票涨价题材反查", "theme master + matching zjtc1/zjtc2",
     "market + code", "source-rank", "source-rank"},
    {"catalog", "功能目录", "none", "none", "source-rank", "source-rank"},
}};

constexpr bool unique_views() {
    for (std::size_t left = 0; left < views.size(); ++left)
        for (std::size_t right = left + 1; right < views.size(); ++right)
            if (views[left].view == views[right].view) return false;
    return true;
}

static_assert(unique_views());

}  // namespace

const std::array<ViewDefinition, 7>& view_definitions() { return views; }

const ViewDefinition* find_view_definition(std::string_view view) {
    const auto found = std::find_if(views.begin(), views.end(),
        [view](const ViewDefinition& definition) {
            return definition.view == view;
        });
    return found == views.end() ? nullptr : &*found;
}

bool view_allows_sort(const ViewDefinition& definition, std::string_view sort) {
    std::size_t start = 0;
    while (start <= definition.allowed_sorts.size()) {
        const auto end = definition.allowed_sorts.find('|', start);
        const auto item = definition.allowed_sorts.substr(
            start, end == std::string_view::npos ? std::string_view::npos : end - start);
        if (item == sort) return true;
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return false;
}

const std::string& commodity_master_resource() {
    static const std::string resource = "list/func_zjtc103_1.jsn";
    return resource;
}

const std::string& theme_master_resource() {
    static const std::string resource = "list/func_zjtc101_1.jsn";
    return resource;
}

Json catalog_rows() {
    Json result = Json::array();
    for (const auto& definition : views) {
        if (definition.view == "catalog") continue;
        Json row = Json::object();
        row["view"] = std::string(definition.view);
        row["label"] = std::string(definition.label);
        row["resources"] = std::string(definition.resources);
        row["selection"] = std::string(definition.selection);
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace tdx::commodity_links_detail
