#include "strong_stocks_internal.hpp"

#include "tdx/common.hpp"

#include <array>
#include <string_view>

namespace tdx::detail::strong_stocks {
namespace {

constexpr std::string_view kMainResource = "list/func_ygzl101_1.jsn";
constexpr std::array<ViewDefinition, 4> kViews{{
    {View::intervals, "intervals", "历史强势股区间", kMainResource,
     "security,start/end,trading-days,limit-up-days,stock/index/excess-return",
     "end-date", "desc", SortFamily::interval},
    {View::security, "security", "单票强势生命周期", kMainResource,
     "all matching intervals", "end-date", "desc", SortFamily::interval},
    {View::detail, "detail", "区间逐日明细", "ygzl/<interval-id>.jsn",
     "stock return/amount/reason,market limit-up/broken/down,sealing,index return",
     "date", "asc", SortFamily::detail},
    {View::catalog, "catalog", "强势股数据目录", "", "available views",
     "", "", SortFamily::none},
}};
constexpr std::array<std::string_view, 9> kIntervalSorts{{
    "end-date", "start-date", "return", "index-return", "excess-return",
    "days", "limit-ups", "source-rank", "code",
}};
constexpr std::array<std::string_view, 5> kDetailSorts{{
    "date", "return", "amount", "market-limit-ups", "seal-rate",
}};

template <std::size_t Size>
bool contains(const std::array<std::string_view, Size>& values, std::string_view name) {
    for (const auto value : values) if (value == name) return true;
    return false;
}

constexpr bool unique_views() {
    for (std::size_t left = 0; left < kViews.size(); ++left)
        for (std::size_t right = left + 1; right < kViews.size(); ++right)
            if (kViews[left].view == kViews[right].view ||
                kViews[left].name == kViews[right].name) return false;
    return true;
}

static_assert(unique_views(), "strong-stock views must be unique");

}  // namespace

const ViewDefinition& view_definition(std::string_view name) {
    for (const auto& definition : kViews) if (definition.name == name) return definition;
    throw Error("view must be intervals, security, detail, or catalog");
}

bool valid_view(std::string_view name) {
    for (const auto& definition : kViews) if (definition.name == name) return true;
    return false;
}

bool valid_sort(SortFamily family, std::string_view name) {
    if (family == SortFamily::interval) return contains(kIntervalSorts, name);
    if (family == SortFamily::detail) return contains(kDetailSorts, name);
    return false;
}

std::string_view main_resource() { return kMainResource; }

Json catalog_rows() {
    Json result = Json::array();
    for (const auto& definition : kViews) {
        if (definition.view == View::catalog) continue;
        Json row = Json::object();
        row["view"] = std::string(definition.name);
        row["label"] = std::string(definition.label);
        row["resource"] = std::string(definition.resource);
        row["fields"] = std::string(definition.fields);
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace tdx::detail::strong_stocks

