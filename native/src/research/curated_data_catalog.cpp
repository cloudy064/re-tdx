#include "tdx/curated_data.hpp"
#include "tdx/curated_data_internal.hpp"
#include "tdx/common.hpp"

namespace tdx {
namespace detail {
namespace curated_data {

constexpr const char* kMedia = "list/func_cmyl101_1.jsn";
constexpr const char* kLowValuation = "list/func_dgzxz101.jsn";
constexpr const char* kDividendFundraising = "list/func_fhmz101_1.jsn";
constexpr const char* kBuybackStatistics = "list/func_gfhgtj101_1.jsn";
constexpr const char* kHighDividend = "list/func_gfhl101_1.jsn";
constexpr const char* kHongKongPerformance = "list/func_ggthq101_1.jsn";
constexpr const char* kHighRefinancingLending = "list/func_ggzrt101_1.jsn";
constexpr const char* kBelowBookSoe = "list/func_gqpjg101_1.jsn";

const ResourceEntry resources[] = {
    {kMedia, "media-entertainment", "传媒娱乐"},
    {kLowValuation, "low-valuation-smallcap", "低估值袖珍股"},
    {kDividendFundraising, "dividend-fundraising", "分红募资统计"},
    {kBuybackStatistics, "buyback-statistics", "拟回购统计"},
    {kHighDividend, "high-dividend", "高分红"},
    {kHongKongPerformance, "hk-performance", "港股多周期表现"},
    {kHighRefinancingLending, "high-refinancing-lending", "转融券余额高"},
    {kBelowBookSoe, "below-book-soe", "破净国企股"}
};

const std::size_t resource_count = 8;

std::string kind_for(const std::string& resource) {
    for (std::size_t i = 0; i < resource_count; ++i)
        if (resources[i].path == resource) return resources[i].kind;
    throw Error("unknown curated-data resource: " + resource);
}

std::string label_for(const std::string& kind) {
    for (std::size_t i = 0; i < resource_count; ++i)
        if (resources[i].kind == kind) return resources[i].label;
    return "未知分类";
}

}  // namespace curated_data
}  // namespace detail
}  // namespace tdx
